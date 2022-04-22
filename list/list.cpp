#include <iostream>
#include <memory>
#include <iterator>
template<size_t N>
class StackStorage {
 public:
  StackStorage(const StackStorage& s) = delete;
  StackStorage() { arr_ = new uint8_t[N]; }
  ~StackStorage() { delete[] arr_; }

  void* allocate(size_t n, size_t allign) {
    int start_begin = begin_;
    if (begin_ % allign) {
      begin_ += (allign - begin_ % allign);
    }
    if (begin_ + n >= N) {
      begin_ = start_begin;
      throw std::bad_alloc();
    }

    begin_ += n;
    return (arr_ + begin_ - n);
  }

  void deallocate(const void*, size_t) {}

 private:
  uint8_t* arr_;
  uint64_t begin_ = 0;
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

  void deallocate(T*, size_t) {}

  template<class U>
  struct rebind {
    typedef StackAllocator<U, N> other;
  };

  StackStorage<N>* GetStorage() const {
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
  using alloc_traits = std::allocator_traits<typename Alloc::template rebind<Node>::other>;
  using my_alloc_type = typename alloc_traits::allocator_type;
  List(const my_alloc_type& alloc = my_alloc_type() ) : alloc_(alloc), fake_node_(new BaseNode){
    fake_node_->prev = reinterpret_cast<Node*>(fake_node_);
    fake_node_->next = reinterpret_cast<Node*>(fake_node_);
  }

  List(size_t n, const my_alloc_type& alloc_out = my_alloc_type()) : alloc_(alloc_out), fake_node_(new BaseNode) {
    total_size_ = n;
    fake_node_->prev = reinterpret_cast<Node*>(fake_node_);
    fake_node_->next = reinterpret_cast<Node*>(fake_node_);
    Node* next = reinterpret_cast<Node*>(fake_node_);
    for (size_t i = 0; i < n; ++i) {
      Node* cur;
      try {
        cur = alloc_traits::allocate(alloc_, 1);
      } catch (...) {
        clear();
        delete fake_node_;
        throw;
      }
      try {
        alloc_traits::construct(alloc_, cur);
      } catch (...) {
        clear();
        delete fake_node_;
        throw;
      }
      //всьавляем начиная с головы и идя в хвост
      InsertAftrerNode(next, cur);
      next = cur;
    }
  }

  List(size_t n, const T& t, my_alloc_type& alloc_out = my_alloc_type()) : alloc_(alloc_out), fake_node_(new BaseNode) {
    total_size_ = n;
    fake_node_->prev = reinterpret_cast<Node*>(fake_node_);
    fake_node_->next = reinterpret_cast<Node*>(fake_node_);
    Node* next = reinterpret_cast<Node*>(fake_node_);
    for (size_t i = 0; i < n; ++i) {
      Node* cur;
      try {
        cur = alloc_traits::allocate(alloc_, 1);
      } catch (...) {
        clear();
        delete fake_node_;
        throw;
      }
      try {
        alloc_traits::construct(alloc_, cur, t);
      } catch (...) {
        clear();
        delete fake_node_;
        throw;
      }
      //всьавляем начиная с головы и идя в хвост
      InsertAftrerNode(next, cur);
      next = cur;
    }
    fake_node_->next = next;
    next->prev = reinterpret_cast<Node*>(fake_node_);
  }

  ~List() {
    clear();
    delete fake_node_;
  }

  T& front() { return fake_node_->next->val; }
  const T& front() const { return fake_node_->next->val; }
  T& back() { return fake_node_->prev->val; }
  const T& back() const { return fake_node_->prev->val; }

  void push_back(const T& t) {
    Node* new_node = alloc_traits::allocate(alloc_, 1);
    try {
      alloc_traits::construct(alloc_, new_node, t);
    }
    catch (...) {
      alloc_traits::deallocate(alloc_, new_node, 1);
      throw;
    }
    InsertAftrerNode(fake_node_->next, new_node);
    ++total_size_;
  }

  template<typename... Args>
  void emplace_back(Args... args) {
    Node* new_node = alloc_traits::allocate(alloc_, 1);
    try {
      alloc_traits::construct(alloc_, new_node, args...);
    }
    catch (...) {
      alloc_traits::deallocate(alloc_, new_node, 1);
      throw;
    }
    Node* last_node = fake_node_->prev;
    if (last_node) {
      InsertAftrerNode(last_node, new_node);
    } else {
      fake_node_->prev = new_node;
      fake_node_->next = new_node;
      new_node->prev = reinterpret_cast<Node*>(fake_node_);
    }
    ++total_size_;
  }

  void push_front(const T& t) {
    Node* new_node = alloc_traits::allocate(alloc_, 1);
    try {
      alloc_traits::construct(alloc_, new_node, t);
    }
    catch (...) {
      alloc_traits::deallocate(alloc_, new_node, 1);
      throw;
    }
    InsertBeforeNode(fake_node_->prev, new_node);
   ++total_size_;
  }

  template<typename... Args>
  void emplace_front(Args... args) {
    Node* new_node = alloc_traits::allocate(alloc_, 1);
    try {
      alloc_traits::construct(alloc_, new_node, args...);
    }
    catch (...) {
      alloc_traits::deallocate(alloc_, new_node, 1);
      throw;
    }
    Node* head_node = fake_node_->next;
    if (head_node) {
      InsertBeforeNode(head_node, new_node);
    } else {
      fake_node_->prev = new_node;
      fake_node_->next = new_node;
    }
    ++total_size_;
  }

  void pop_back() {
    if (total_size_) {
      Node* last_node = fake_node_->next;
      EraseNode(last_node);
      alloc_traits::destroy(alloc_, last_node);
      alloc_traits::deallocate(alloc_, last_node, 1);
      --total_size_;
    }
  }
  void pop_front() {
    if (total_size_) {
      Node* first_node = fake_node_->prev;
      EraseNode(first_node);
      alloc_traits::destroy(alloc_, first_node);
      alloc_traits::deallocate(alloc_, first_node, 1);
      --total_size_;
    }
  }

  void clear() {
    for (Node* node = fake_node_->prev; node != fake_node_;) {
      Node* prev = node->prev;
      alloc_traits::destroy(alloc_, node);
      alloc_traits::deallocate(alloc_, node, 1);
      node = prev;
    }
    fake_node_->prev = reinterpret_cast<Node*>(fake_node_);
    fake_node_->next = reinterpret_cast<Node*>(fake_node_);
    total_size_ = 0;
  }

  size_t size() const { return total_size_; }
  bool empty() const { return total_size_ == 0; }

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
    Node* new_node = alloc_traits::allocate(alloc_, 1);
    try {
      alloc_traits::construct(alloc_, new_node, t);
    } catch (...) {
      alloc_traits::deallocate(alloc_, new_node, 1);
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
      alloc_traits::destroy(alloc_, pos.GetNodePointer());
      alloc_traits::deallocate(alloc_, cur, 1);
      --total_size_;
    }
  }

  List(const List& lst): total_size_(lst.total_size_), alloc_(alloc_traits::select_on_container_copy_construction(lst.alloc_)), fake_node_(new BaseNode) {
    fake_node_->prev = reinterpret_cast<Node*>(fake_node_);
    fake_node_->next = reinterpret_cast<Node*>(fake_node_);
    Node* next = reinterpret_cast<Node*>(fake_node_);
    for(common_iter it = lst.begin(), _end = lst.end(); it != _end; ++it) {
      Node* cur;
      try {
        cur = alloc_traits::allocate(alloc_, 1);
      } catch (...) {
        clear();
        delete fake_node_;
        throw;
      }
      try {
        alloc_traits::construct(alloc_, cur, *it);
      } catch (...) {
        clear();
        delete fake_node_;
        throw;
      }
      InsertAftrerNode(next, cur);
      next = cur;
    }
  }

  List& operator=(const List& lst){
    if(alloc_traits::propagate_on_container_copy_assignment::value) {
      alloc_ = lst.alloc_;
    }

    BaseNode* new_fake_node = new BaseNode;
    new_fake_node->prev = reinterpret_cast<Node*>(new_fake_node);
    new_fake_node->next = reinterpret_cast<Node*>(new_fake_node);
    Node* next = reinterpret_cast<Node*>(new_fake_node);
    for(common_iter it = lst.begin(), _end = lst.end(); it != _end; ++it) {
      Node* cur;
      try {
        cur = alloc_traits::allocate(alloc_, 1);
      } catch (...) {
        for (Node* new_cur = new_fake_node->prev; new_cur != new_fake_node;) {
          Node* prev = new_cur->prev;
          alloc_traits::destroy(alloc_, new_cur);
          alloc_traits::deallocate(alloc_, new_cur, 1);
          new_cur = prev;
        }
        delete new_fake_node;
        throw;
      }
      try {
        alloc_traits::construct(alloc_, cur, *it);
      } catch (...) {
        for (Node* new_cur = new_fake_node->prev; new_cur != new_fake_node;) {
          Node* prev = new_cur->prev;
          alloc_traits::destroy(alloc_, new_cur);
          alloc_traits::deallocate(alloc_, new_cur, 1);
          new_cur = prev;
        }
        delete new_fake_node;
        throw;
      }
      InsertAftrerNode(next, cur);
      next = cur;
    }
    clear();
    delete fake_node_;
    fake_node_ = new_fake_node;
    total_size_ = lst.total_size_;
    return *this;
  }


  Alloc get_allocator() const { return alloc_; }


 private:

    struct BaseNode {
    BaseNode() = default;
    ~BaseNode() = default;
    BaseNode(Node* const next, Node* const prev) : next(next), prev(prev) {}
    Node* next = nullptr;
    Node* prev = nullptr;
  };

  struct Node : BaseNode {
    Node() = default;
    ~Node() = default;
    Node(Node* const next, Node* const prev, const T& t) : BaseNode(next, prev), val(t) {}
    explicit Node(const T& t) : val(t) {}
    template<typename... Args>
    Node(Args... args): val(args...) {}
    T val;
  };

  size_t total_size_ = 0;
  typename alloc_traits::allocator_type alloc_;
  BaseNode* fake_node_;


  static void InsertNodeBetweenTwo(Node* head, Node* tail, Node* cur) {
    cur->prev = tail;
    cur->next = head;
    tail->next = cur;
    head->prev = cur;
  }

  static void InsertAftrerNode(Node* node, Node* cur) {
    Node* prev = node->prev;
    prev->next = cur;
    cur->prev = prev;
    node->prev = cur;
    cur->next = node;
  }

  static void InsertBeforeNode(Node* node, Node* cur) {
    Node* next = node->next;    // order is essential
    next->prev = cur;
    cur->next = next;
    node->next = cur;
    cur->prev = node;
  }

  static void EraseNode(Node* node) {
    Node* prev = node->prev;
    Node* next = node->next;
    prev->next = next;
    next->prev = prev;
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

