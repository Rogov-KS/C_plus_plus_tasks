#include <iostream>

namespace Const{
const int default_am_array = 3;
const int part_all_deck = 3;
const int kThree = 3;
}
template<bool is_const, typename T, typename D>
struct Conditional {
  typedef D type;
};

template<typename T, typename D>
struct Conditional<true, T, D> {
  typedef T type;
};

template<typename T>
class Deque;

template<typename T>
void swap(Deque<T>& a, Deque<T>& b) noexcept {
  std::swap(a.begin_, b.begin_);
  std::swap(a.arr_pointers_, b.arr_pointers_);
  std::swap(a.am_arr_, b.am_arr_);
  std::swap(a.size_, b.size_);
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const Deque<T>& a) noexcept;

template<typename T>
class Deque {
 public:
  Deque() {
    arr_pointers_ = new T* [Const::default_am_array]; // Exception safety
    am_arr_ = Const::default_am_array;
    size_ = 0;

    default_array();

    begin_ = am_arr_ * size_of_arr_ / Const::default_am_array; // *3 / 3 ????
  }

  Deque(const Deque& other) : am_arr_(other.am_arr_), begin_(other.begin_), size_(other.size_) {
    CreatCopyArr(other.arr_pointers_);
  }

  ~Deque() {
    for (iterator it = begin(); it < end(); ++it) {
      it->~T();
    }
    if(arr_pointers_) {
      for (size_t i = 0; i < am_arr_; ++i) {
        delete[] reinterpret_cast<uint8_t*>(arr_pointers_[i]);
      }
      delete[] arr_pointers_;
    }
  }

  explicit Deque(int x) {  // Objects not constructed. Use next constructor here
    am_arr_ = Const::part_all_deck * (x / size_of_arr_ + (x % size_of_arr_ != 0));
    arr_pointers_ = new T* [am_arr_];  // safety
    size_ = x;

    default_array();

    begin_ = am_arr_ * size_of_arr_ / Const::part_all_deck;
  }

  Deque(size_t x, const T& copy) {
    am_arr_ = Const::part_all_deck * (x / size_of_arr_ + ((x % size_of_arr_) != 0));
    begin_ = am_arr_ * size_of_arr_ / Const::part_all_deck;
    arr_pointers_ = new T* [am_arr_]; // safety
    size_ = x;
    for (size_t i = 0; i < am_arr_; ++i) {
      try {
        arr_pointers_[i] = reinterpret_cast<T*>(new uint8_t[sizeof(T) * size_of_arr_]);
      } catch(...) {
        free_arr_pointers_until_index(i);
        throw;
      }
    }

    size_t s = 0;
    for (size_t i = am_arr_ / Const::part_all_deck; i < (Const::part_all_deck - 1) * am_arr_ / Const::part_all_deck; ++i) {
      for (size_t j = 0; j < size_of_arr_; ++j) {
        try {
          new(arr_pointers_[i] + j) T(copy); //execption
        } catch (...) {
          free_arr_pointers();
          throw;
        }
        ++s;
        if (s == x) {
          break;
        }
      }
      if (s == x) {
        break;
      }
    }
  }

  Deque(const std::initializer_list<T>& list) {
    size_ = list.size();
    am_arr_ = Const::part_all_deck * (size_ / size_of_arr_ + (size_ % size_of_arr_ != 0));
    arr_pointers_ = new T* [am_arr_]; // safety
    begin_ = am_arr_ * size_of_arr_ / Const::part_all_deck;
    size_t s = 0;
    for (size_t i = 0; i < am_arr_; ++i) {
      try {
        arr_pointers_[i] = reinterpret_cast<T*>(new uint8_t[sizeof(T) * size_of_arr_]);
      } catch (...) {
        free_arr_pointers_until_index(i);
      }
    }
    for (auto t : list) {
      try {
        new(&arr_pointers_[s]) T(t);  // hard to read. Use []
      } catch (...) {
        free_arr_pointers();
      }
      ++s;
    }
  }

  Deque& operator=(const Deque& D) {
    Deque copy(D);
    begin_ = copy.begin_;
    size_ = copy.size_;
    am_arr_ = copy.am_arr_;
    arr_pointers_ = copy.arr_pointers_;
    copy.arr_pointers_ = nullptr;
    return *this;
  }

  size_t size() const noexcept {
    return size_;
  }

  T& operator[](size_t i) noexcept {
    return arr_pointers_[(begin_ + i) / size_of_arr_][(begin_ + i) % size_of_arr_];
  }

  T& at(size_t i) {
    if (i >= size_) {
      throw std::out_of_range("out_of_range");
    }
    return (*this)[i]; // use []
  }

  const T& operator[](size_t i) const noexcept {
    return (*this)[i];
  }

  const T& at(size_t i) const {
    if (i >= size_) {
      throw std::out_of_range("out_of_range");
    }
    return (*this)[i]; //
  }

  void push_back(const T& t) {
    if (begin_ + size_ >= am_arr_ * size_of_arr_) {
      relocate();
    }

    (*this)[size_] = t;

    ++size_;
  }

  void pop_back() {
    if (size_ > 0) {
      (&(*this)[size_ - 1])->~T();
      --size_;
    }
  }

  void push_front(const T& t) {
    if (begin_ == 0) {
      relocate();
    }

    (*this)[-1] = t;

    --begin_;
    ++size_;
  }

  void pop_front() {
    if (size_ > 0) {
      (&(*this)[0])->~T();
      ++begin_;
      --size_;
    }
  }

  T front() {
    return (*this)[0]; // []
  }

  const T& front() const {
    return (*this)[0]; // []
  }

  T back() {
    return (*this)[size_ - 1];  // []
  }

  const T& back() const {
    return (*this)[size_ - 1]; // []
  }

  template<bool is_const>
  struct common_iter;

  using iterator = common_iter<false>;
  using const_iterator = common_iter<true>;

  template<bool is_const>
  struct common_iter {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = long long;
    using pointer = typename Conditional<is_const, const T*, T*>::type;
    using reference = typename Conditional<is_const, const T&, T&>::type;

    common_iter() = default;
    common_iter(const common_iter& it_) = default;

    common_iter(T** const kP, T* const kElem) : arr_(kP), elem_(kElem) {}

    common_iter& operator=(const common_iter& it) = default;

    ~common_iter() = default;

    common_iter& operator++() {
      if (elem_ - *arr_ == size_of_arr_ - 1) {
        ++arr_;
        elem_ = arr_[0];
      } else {
        ++elem_;
      }
      return *this;
    }

    common_iter operator++(int) {
      common_iter copy(*this);
      ++(*this);
      return copy;
    }

    common_iter& operator--() {
      if (elem_ == *arr_) {
        --arr_;
        elem_ = arr_[0] + size_of_arr_ - 1;
      } else {
        --elem_;
      }
      return *this;
    }

    common_iter operator--(int) {
      common_iter copy(*this);
      --(*this);
      return copy;
    }

    difference_type operator-(const common_iter& it) const {
      return (arr_ - it.arr_) * size_of_arr_ + (elem_ - *arr_) - (it.elem_ - *it.arr_);
    }

    common_iter operator+=(difference_type dif) {
      if (dif == 0) {
        return *this;
      }
      dif = (dif + (elem_ - *arr_));
      if (dif > 0) {
        arr_ += (dif / size_of_arr_);
        elem_ = (*arr_ + dif % size_of_arr_);
      } else {
        difference_type dif2 = static_cast<size_t>(size_of_arr_) - 1 - dif;
        arr_ -= dif2 / size_of_arr_;
        elem_ = *arr_ + (size_of_arr_ - 1 - (dif2 % size_of_arr_));
      }
      return *this;
    }

    common_iter operator+(difference_type a) {
      common_iter copy(*this);
      return copy += a;
    }

    common_iter operator-=(difference_type a) {
      *this += (-a);
      return *this;
    }

    common_iter operator-(difference_type a) {
      common_iter copy(*this);
      return copy -= a;
    }

    reference operator*() {
      return *elem_;
    }

    reference operator*() const {
      return *elem_;
    }

    pointer operator->() {
      return elem_;
    }

    operator const_iterator() const {   //NOLINT
      const_iterator it(arr_, elem_);
      return it;
    }

    bool operator>(const common_iter& it) const {
      difference_type h = (*this - it);
      return h > 0;
    }

    bool operator<(const common_iter& it) const {
      return (it > *this);
    }

    bool operator==(const common_iter& it) const {
      return (elem_ == it.elem_);
    }

    bool operator!=(const common_iter& it) const {
      return !(*this == it);
    }

    bool operator>=(const common_iter& it) const {
      return !(*this < it);
    }

    bool operator<=(const common_iter& it) const {
      return !(*this > it);
    }

   private:
    T** arr_;
    T* elem_;
  };

  iterator begin() {
    iterator it(&arr_pointers_[begin_ / size_of_arr_], &arr_pointers_[begin_ / size_of_arr_][begin_ % size_of_arr_]);
    return it;
  }

  iterator end() {
    iterator it(&arr_pointers_[(begin_ + size_) / size_of_arr_],
                &arr_pointers_[(begin_ + size_) / size_of_arr_][(begin_ + size_) % size_of_arr_]);
    return it;
  }

  const_iterator begin() const {
    const_iterator it(&arr_pointers_[begin_ / size_of_arr_], &arr_pointers_[begin_ / size_of_arr_][begin_ % size_of_arr_]);
    return it;
  }

  const_iterator end() const {
    const_iterator it(&arr_pointers_[(begin_ + size_) / size_of_arr_],
                      &arr_pointers_[(begin_ + size_) / size_of_arr_][(begin_ + size_) % size_of_arr_]);
    return it;
  }

  const_iterator cbegin() {
    iterator it = begin(); // begin + cast ?
    return it;
  }

  const_iterator cend() {
    iterator it = end();
    return it;
  }

  template <bool is_const>
  using common_reverse_iterator = std::reverse_iterator<common_iter<is_const>>;

  using reverse_iterator = common_reverse_iterator<false>;
  using const_reverse_iterator = common_reverse_iterator<true>;

  reverse_iterator rbegin() {
    return (static_cast<reverse_iterator>(this->end()));
  }

  reverse_iterator rend() {
    return static_cast<reverse_iterator>(begin());
  }

  const_reverse_iterator rbegin() const {
    return static_cast<const_reverse_iterator>(*this->end());
  }

  const_reverse_iterator rend() const {
    return static_cast<const_reverse_iterator>(this->begin());
  }

  const_reverse_iterator crbegin() {
    return static_cast<const_reverse_iterator>(*this->end());
  }

  const_reverse_iterator crend() {
    return static_cast<const_reverse_iterator>(this->begin());
  }

  void insert(iterator pos, const T& t) {                  // ?????
    Deque copy;
    try {
      copy = *this;
    } catch (...) {
      throw;
    }

    try {
      if (copy.size_ + copy.begin_ == copy.am_arr_ * size_of_arr_) {
        copy.relocate();
      }
      ++copy.size_;
      auto copy_pos = copy.begin() + (pos - begin());
      auto lim = --copy.end();
      for (auto it = lim; it > copy_pos; --it) {
        auto ret = *(it - 1);
        *it = ret;
      }
      *copy_pos = t;
    } catch (...) {
      throw;
    }
    swap(*this, copy);
  }

  void erase(iterator pos) {
    Deque copy;
    try {
      copy = *this;
    } catch (...) {
      throw;
    }

    try {
      auto copy_pos = copy.begin() + (pos - begin());
      auto lim = --copy.end();
      for (auto it = copy_pos; it < lim; ++it) {
        auto ret = *(it + 1);
        *(it) = ret;
      }
      --copy.size_;
      (lim)->~T();
      if (copy.size_ < size_of_arr_ * am_arr_ / 9) {
        copy.relocate();
      }
    } catch (...) {
      throw;
    }
    swap(*this, copy);
  }

 private:

  static const uint64_t size_of_arr_ = 64;
  T** arr_pointers_;
  uint64_t am_arr_;
  uint64_t begin_;
  uint64_t size_;

  void CreatCopyArr(T** arr_copy) {
    arr_pointers_ = new T* [am_arr_]; // safety
    for (size_t i = 0; i < am_arr_; ++i) {
      try{
        arr_pointers_[i] = reinterpret_cast<T*>(new uint8_t[size_of_arr_ * sizeof(T)]);
      } catch(...) {
        free_arr_pointers_until_index(i);
        throw;
      }
    }
    size_t a, b;
    for (size_t i = 0; i < size_; ++i) {
      a = (begin_ + i) / size_of_arr_;
      b = (begin_ + i) % size_of_arr_;
      try {
        new(arr_pointers_[a] + b) T(arr_copy[a][b]);
      } catch(...) {
        free_arr_pointers();
      }
    }
  }

  void GetNewParaments(size_t new_am_arr, T** new_arr) {
    size_t beginer = begin_ / size_of_arr_;
    size_t ender = (begin_ + size_ - 1) / size_of_arr_;
    for (size_t i = 0; i < beginer; ++i) {
      delete[] reinterpret_cast<uint8_t*>(arr_pointers_[i]);
    }
    for (size_t i = ender + 1; i < am_arr_; ++i) {
      delete[] reinterpret_cast<uint8_t*>(arr_pointers_[i]);
    }
    delete[] arr_pointers_;
    am_arr_ = new_am_arr;
    arr_pointers_ = new_arr;
    begin_ = begin_ % size_of_arr_ + new_am_arr * size_of_arr_ / Const::part_all_deck;
  }

  void relocate() {   // No safety fix
    size_t beginer = begin_ / size_of_arr_;
    size_t ender = (begin_ + size_ - 1) / size_of_arr_;
    size_t new_am_arr = Const::part_all_deck * (ender - beginer + 1);
    T** new_arr = new T* [new_am_arr];
    for (size_t i = 0; i < new_am_arr; ++i) {
      if (i < new_am_arr / Const::part_all_deck || i >= (Const::part_all_deck - 1) * new_am_arr / Const::part_all_deck) {
        try {
          new_arr[i] = reinterpret_cast<T*>(new uint8_t[sizeof(T) * size_of_arr_]);
        } catch (...) {
          for (size_t j = 0; j < new_am_arr; ++j) {
            if ((j < new_am_arr / Const::part_all_deck || j >= (Const::part_all_deck - 1) * new_am_arr / Const::part_all_deck) && j <= i) {
              delete[] reinterpret_cast<uint8_t*>(new_arr[j]);
            }
          } // Unreachable code fix
          delete[] new_arr; // Free memory correctly
          throw;
        }
      } else {
        new_arr[i] = arr_pointers_[i - new_am_arr / Const::part_all_deck + beginer];
      }
    }
    GetNewParaments(new_am_arr, new_arr);
  }

  void free_arr_pointers_until_index(size_t index) noexcept {
    for(size_t j = 0; j < index; ++j) {
      delete[] arr_pointers_[j];
    }
    delete[] arr_pointers_;
  }

  void free_arr_pointers() {
    free_arr_pointers_until_index(am_arr_);
  }

  void default_array(const T& tmp = T()) {
    for (size_t i = 0; i < am_arr_; ++i) {
      try {
        arr_pointers_[i] = reinterpret_cast<T*>(new uint8_t[sizeof(T) * size_of_arr_]);
      } catch (...) {
        free_arr_pointers_until_index(i);
        throw;
      }
    }

    for (size_t i = 0; i < am_arr_; ++i) {
      try {
        new(arr_pointers_ + i) T(tmp);
      } catch (...) {
        free_arr_pointers();
        throw;
      }
    }
  }

  friend void swap <>(Deque<T>& a, Deque<T>& b) noexcept;
};


template<typename T>
std::ostream& operator<<(std::ostream& os, const Deque<T>& a) noexcept {
  for (auto v : a) {
    os << v << " ";
  }
  os << '\n';
  return os;
}
