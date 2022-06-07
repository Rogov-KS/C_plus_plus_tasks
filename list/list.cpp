#include <iostream>
#include <memory>
#include <iterator>
template<size_t N>
class StackStorage {
 public:
  StackStorage(const StackStorage& s) = delete;
  StackStorage() : arr_(new uint8_t[N]), begin_(arr_), sz(N) {}
  ~StackStorage() { delete[] arr_; }

  void* allocate(size_t n, size_t allign) {
    auto pointer = std::align(allign, n, begin_, sz);
    begin_ = static_cast<char*>(begin_) + n;
    sz -= n;
    if (!pointer) {
      throw std::bad_alloc();
    }

    return pointer;
  }

  void deallocate(const void*, size_t) noexcept {}

 private:
  uint8_t* arr_;
  void* begin_;
  size_t sz;
};

template<typename T, size_t N>
class StackAllocator {
 public:

  explicit StackAllocator(StackStorage<N>& arr) : storage_(&arr) {}
  StackAllocator() {}

  template<typename U>
  StackAllocator(const StackAllocator<U, N>& alloc): storage_(alloc.GetStorage()) {}
  template<typename U>
  StackAllocator& operator=(const StackAllocator<U, N>& alloc) {
    storage_ = alloc.GetStorage();
    return *this;
  }

  T* allocate(size_t n) {
    return reinterpret_cast<T*>(storage_->allocate(n * sizeof(T), alignof(T)));
  }

  void deallocate(T*, size_t) noexcept {}

  template<class U>
  struct rebind {
    typedef StackAllocator<U, N> other;
  };

  StackStorage<N>* GetStorage() const noexcept {
    return storage_;
  }


  static StackAllocator<T, N> select_on_container_copy_construction(const StackAllocator<T, N>& alloc) {
    return alloc;
  }

  using value_type = T;
  struct propagate_on_container_copy_assignment: std::false_type {};
  using is_always_equal = std::true_type;
 private:
  StackStorage<N>* storage_;
};

template<typename T, typename U, size_t N>
bool operator==(const StackAllocator<T, N>& alloc1, const StackAllocator<U, N>& alloc2) {
  return alloc1.storage_ == alloc2.storage_;
}
template<typename T, typename U, size_t N>
bool operator!=(const StackAllocator<T, N>& alloc1, const StackAllocator<U, N>& alloc2) {
  return !(alloc1 == alloc2);
}

template<typename T, typename Alloc = std::allocator<T>>
class List {
 private:
  struct BaseNode;
  struct Node;
 public:
  using node_alloc = std::allocator_traits<typename Alloc::template rebind<Node>::other>;
  using my_alloc_type = typename node_alloc::allocator_type;
  List(const my_alloc_type& alloc = my_alloc_type() ) : alloc_(alloc), fake_node_(new BaseNode){
    fake_node_->prev = reinterpret_cast<Node*>(fake_node_);
    fake_node_->next = reinterpret_cast<Node*>(fake_node_);
  }

  List(size_t n, const my_alloc_type& alloc_out = my_alloc_type()) : List(alloc_out), total_size_(n){
    auto next = fake_node_;
    for (size_t i = 0; i < n; ++i) {
      copy_elem(T(), next, fake_node_);
    }
  }

  List(size_t n, const T& t, my_alloc_type& alloc_out = my_alloc_type()) : List(alloc_), total_size_(n) {
    auto next = fake_node_;
    for (size_t i = 0; i < n; ++i) {
      copy_elem(t, next, fake_node_);
    }
    fake_node_->next = next;
    next->prev = fake_node_;
  }

  ~List() noexcept {
    clear();
    delete fake_node_;
  }

  T& front() noexcept { return fake_node_->next->val; }
  const T& front() const noexcept { return fake_node_->next->val; }
  T& back() noexcept { return fake_node_->prev->val; }
  const T& back() const noexcept { return fake_node_->prev->val; }

  void push_back(const T& t) {
    Node* new_node = node_alloc::allocate(alloc_, 1);
    try {
      node_alloc::construct(alloc_, new_node, t);
    }
    catch (...) {
      node_alloc::deallocate(alloc_, new_node, 1);
      throw;
    }
    InsertAftrerNode(fake_node_->next, new_node);
    ++total_size_;
  }

  template<typename... Args>
  void emplace_back(Args... args) {
    Node* new_node = node_alloc::allocate(alloc_, 1);
    try {
      node_alloc::construct(alloc_, new_node, args...);
    }
    catch (...) {
      node_alloc::deallocate(alloc_, new_node, 1);
      throw;
    }
    Node* last_node = fake_node_->prev;
    InsertAftrerNode(last_node, new_node);
    ++total_size_;
  }

  void push_front(const T& t) {
    Node* new_node = node_alloc::allocate(alloc_, 1);
    try {
      node_alloc::construct(alloc_, new_node, t);
    }
    catch (...) {
      node_alloc::deallocate(alloc_, new_node, 1);
      throw;
    }
    InsertBeforeNode(fake_node_->prev, new_node);
   ++total_size_;
  }

  template<typename... Args>
  void emplace_front(Args... args) {
    Node* new_node = node_alloc::allocate(alloc_, 1);
    try {
      node_alloc::construct(alloc_, new_node, args...);
    }
    catch (...) {
      node_alloc::deallocate(alloc_, new_node, 1);
      throw;
    }
    InsertBeforeNode(fake_node_->next, new_node);


    ++total_size_;
  }

  void pop_back() {
    if (total_size_) {
      BaseNode* last_node = fake_node_->next;
      EraseNode(last_node);
      node_alloc::destroy(alloc_, static_cast<Node*>(last_node));
      node_alloc::deallocate(alloc_, static_cast<Node*>(last_node), 1);
      --total_size_;
    }
  }
  void pop_front() {
    if (total_size_) {
      Node* first_node = static_cast<Node*>(fake_node_->prev);
      EraseNode(first_node);
      node_alloc::destroy(alloc_, first_node);
      node_alloc::deallocate(alloc_, first_node, 1);
      --total_size_;
    }
  }

  void clear() noexcept {
    for (Node* node = static_cast<Node*>(fake_node_->prev); node != fake_node_;) {
      auto prev = static_cast<Node*>(node->prev);
      node_alloc::destroy(alloc_, node);
      node_alloc::deallocate(alloc_, node, 1);
      node = prev;
    }
    fake_node_->prev = fake_node_;
    fake_node_->next = fake_node_;
    total_size_ = 0;
  }


  size_t size() const noexcept { return total_size_; }
  bool empty() const noexcept { return total_size_ == 0; }

  template<bool is_const>
  struct common_iter;

  using iterator = common_iter<false>;
  using const_iterator = common_iter<true>;

  template<bool is_const>
  struct common_iter {
    using value_type = T;
    using reference = typename std::conditional<is_const, const T&, T&>::type;
    using pointer = typename std::conditional<is_const, const T*, T*>::type;
    using difference_type = int64_t;
    using iterator_category = std::bidirectional_iterator_tag;

    reference operator*() const { return cur->val; }
    pointer operator->() const { return &(cur->val); }
    Node* GetNodePointer() const { return cur; }

    common_iter() = default;
    explicit common_iter(Node* const node) : cur(node) {}
    common_iter(const common_iter& it) : cur(it.cur) {}
    ~common_iter() = default;
    operator const_iterator() {
      const_iterator it(GetNodePointer());
      return it;
    }

    common_iter operator++() {
      cur = cur->prev;
      return *this;
    }
    common_iter operator++(int) {
      auto copy(*this);
      ++*this;
      return copy;
    }

    common_iter operator--() {
      cur = cur->next;
      return *this;
    }
    common_iter operator--(int) {
      auto copy(*this);
      --*this;
      return copy;
    }

    bool operator==(const common_iter& it) const { return cur == it.cur; }
    bool operator!=(const common_iter& it) const { return cur != it.cur; }

   private:
    Node* cur;
  };

  iterator begin() {
    iterator it(fake_node_->prev);
    return it;
  }
  iterator end() {
    iterator it(reinterpret_cast<Node*>(fake_node_));
    return it;
  }
  const_iterator begin() const {
    iterator it(fake_node_->prev);
    return it;
  }
  const_iterator end() const {
    iterator it(reinterpret_cast<Node*>(fake_node_));
    return it;
  }
  const_iterator cbegin() const {
    iterator it(fake_node_->prev);
    return it;
  }
  const_iterator cend() const {
    iterator it(reinterpret_cast<Node*>(fake_node_));
    return it;
  }

  template<bool is_const>
  using common_reverse_iterator = std::reverse_iterator<common_iter<is_const>>;
  using reverse_iterator = common_reverse_iterator<false>;
  using const_reverse_iterator = common_reverse_iterator<true>;

  reverse_iterator rbegin() { return std::make_reverse_iterator(end()); }
  reverse_iterator rend() { return std::make_reverse_iterator(begin()); }

  const_reverse_iterator rbegin() const { return std::make_reverse_iterator(cend()); }
  const_reverse_iterator rend() const { return std::make_reverse_iterator(cbegin()); }

  const_reverse_iterator crbegin() const { return std::make_reverse_iterator(cend()); }
  const_reverse_iterator crend() const { return std::make_reverse_iterator(cbegin()); }

  void insert(const const_iterator& pos, const T& t) {
    Node* new_node = node_alloc::allocate(alloc_, 1);
    try {
      node_alloc::construct(alloc_, new_node, t);
    } catch (...) {
      node_alloc::deallocate(alloc_, new_node, 1);
      throw;
    }
    Node* cur = pos.GetNodePointer();
    InsertBeforeNode(cur, new_node);
    ++total_size_;
  }

  void erase(const const_iterator& pos) {
    if(total_size_) {
      Node* cur = pos.GetNodePointer();
      EraseNode(cur);
      node_alloc::destroy(alloc_, pos.GetNodePointer());
      node_alloc::deallocate(alloc_, cur, 1);
      --total_size_;
    }
  }

  List(const List& lst): List(node_alloc::select_on_container_copy_construction(lst.alloc_)), total_size_(lst.total_size_) {
    auto next = fake_node_;
    for(common_iter it = lst.begin(), _end = lst.end(); it != _end; ++it) {
      copy_elem(*it, next, fake_node_);
    }
  }

  List& operator=(const List& lst){
    if(node_alloc::propagate_on_container_copy_assignment::value) {
      alloc_ = lst.alloc_;
    }

    auto new_fake_node = new BaseNode;
    new_fake_node->prev = new_fake_node;
    new_fake_node->next = new_fake_node;
    BaseNode* next = new_fake_node;
    for(common_iter it = lst.begin(), _end = lst.end(); it != _end; ++it) {
      copy_elem(*it, next, new_fake_node);
    }
    clear();
    delete fake_node_;
    fake_node_ = new_fake_node;
    total_size_ = lst.total_size_;
    return *this;
  }


  Alloc get_allocator() const noexcept { return alloc_; }


 private:

    struct BaseNode {
    BaseNode() = default;
    ~BaseNode() = default;
    BaseNode(BaseNode* const next, BaseNode* const prev) : next(next), prev(prev) {}
    BaseNode* next = nullptr;
    BaseNode* prev = nullptr;
  };

  struct Node : BaseNode {
    Node() = default;
    ~Node() = default;
    Node(BaseNode* const next, BaseNode* const prev, const T& t) : BaseNode(next, prev), val(t) {}
    T Val() const noexcept { return val; }
    explicit Node(const T& t) : val(t) {}
    template<typename... Args>
    Node(Args... args): val(args...) {}
    T val;
  };

  size_t total_size_ = 0;
  typename node_alloc::allocator_type alloc_;
  BaseNode* fake_node_;


  static void InsertNodeBetweenTwo(BaseNode* head, BaseNode* tail, BaseNode* cur) {
    cur->prev = tail;
    cur->next = head;
    tail->next = cur;
    head->prev = cur;
  }

  static void InsertAftrerNode(BaseNode* node, BaseNode* cur) {
    BaseNode* prev = node->prev;
    prev->next = cur;
    cur->prev = prev;
    node->prev = cur;
    cur->next = node;
  }

  static void InsertBeforeNode(BaseNode* node, BaseNode* cur) {
    BaseNode* next = node->next;    // order is essential
    next->prev = cur;
    cur->next = next;
    node->next = cur;
    cur->prev = node;
  }

  static void EraseNode(BaseNode* node) {
    BaseNode* prev = node->prev;
    BaseNode* next = node->next;
    prev->next = next;
    next->prev = prev;
  }

  void clear_fake(BaseNode* fake_node) {
    for (Node* node = static_cast<Node*>(fake_node->prev); node != fake_node;) {
      auto prev = static_cast<Node*>(node->prev);
      node_alloc::destroy(alloc_, node);
      node_alloc::deallocate(alloc_, static_cast<Node*>(node), 1);
      node = prev;
    }
  }

  void copy_elem(const T& tmp, BaseNode* next, BaseNode* fake_node) {
    Node* cur;
    try {
      cur = node_alloc::allocate(alloc_, 1);
    } catch (...) {
      for (Node* new_cur = fake_node->prev; new_cur != fake_node;) {
        Node* prev = new_cur->prev;
        node_alloc::destroy(alloc_, new_cur);
        node_alloc::deallocate(alloc_, new_cur, 1);
        new_cur = prev;
      }
      delete fake_node;
      delete cur;
      throw;
    }
    try {
      node_alloc::construct(alloc_, cur, tmp);
    } catch (...) {
      for (Node* new_cur = fake_node->prev; new_cur != fake_node;) {
        Node* prev = new_cur->prev;
        node_alloc::destroy(alloc_, new_cur);
        node_alloc::deallocate(alloc_, new_cur, 1);
        new_cur = prev;
      }
      node_alloc::destroy(alloc_, cur);
      node_alloc::deallocate(alloc_, cur);
      delete fake_node;
      throw;
    }
    InsertAftrerNode(next, cur);
    next = cur;
  }
};

template<typename T, typename Alloc = std::allocator<T>>
std::ostream& operator<<(std::ostream& os, const List<T>& lst) {
  for (auto it = lst.begin(), _end = lst.end(); it != _end; ++it) {
    os << *it << " ";
  }
  os << "\n";
  return os;
}

