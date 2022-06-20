#include <iostream>
#include <memory>
#include <type_traits>

template <typename T>
class SharedPtr;

template <typename T>
class WeakPtr;

template <class T, class Alloc = std::allocator<T>, class... Args>
SharedPtr<T> allocateShared(const Alloc& alloc = std::allocator<T>(), Args&& ... args);


template <typename T>
class SharedPtr {
 public:

  SharedPtr() noexcept : pb_(nullptr), val_(nullptr) {}

  template <typename U, typename Deleter = std::default_delete<U>, typename Alloc = std::allocator<U>>
  SharedPtr(U* ptr, Deleter del = Deleter(), const Alloc& alloc = Alloc()) {
    using new_type = typename SharedPtr<T>::template DividedControlBlock<U, Deleter, Alloc>;
    using alloc_block_t = typename std::allocator_traits<Alloc>::template rebind_traits<new_type>;
    typename alloc_block_t::allocator_type block_alloc(alloc);

    auto block_ptr = alloc_block_t::allocate(block_alloc, 1);
    try {
      new(block_ptr) new_type(del, alloc, ptr);
    } catch (...) {
      alloc_block_t::deallocate(block_alloc, block_ptr, 1);
    }

    pb_ = block_ptr;
    val_ = ptr;
    if(pb_) pb_->increment_shared_counter();
  }

  SharedPtr(const SharedPtr<T>& other) noexcept : pb_(other.pb_),  val_(other.val_) {
    if(pb_) pb_->increment_shared_counter();
  }
  template <typename U>
  SharedPtr(const SharedPtr<U>& other) noexcept : pb_(reinterpret_cast<BaseControlBlock*>(other.pb_)),  val_(other.val_) {
    if(pb_) pb_->increment_shared_counter();
  }

  SharedPtr(SharedPtr<T>&& other) noexcept : pb_(other.pb_), val_(other.val_) {
    other.ForgetYourSelf();
  }

  template <typename U>
  SharedPtr(SharedPtr<U>&& other) noexcept : pb_(reinterpret_cast<BaseControlBlock*>(other.pb_)), val_(other.val_) {
    other.ForgetYourSelf();
  }

  SharedPtr& operator=(const SharedPtr<T>& other) noexcept {
    DestructorHelper();
    pb_ = other.pb_;
    val_ = other.val_;
    if(pb_) pb_->increment_shared_counter();
    return *this;
  }

  template <typename U>
  SharedPtr& operator=(const SharedPtr<U>& other) noexcept {
    DestructorHelper();
    pb_ = reinterpret_cast<BaseControlBlock*>(other.pb_);
    val_ = other.val_;
    if(pb_) pb_->increment_shared_counter();
    return *this;
  }

  SharedPtr& operator=(SharedPtr<T>&& other) noexcept {
    DestructorHelper();
    pb_ = other.pb_;
    val_ = other.val_;
    other.ForgetYourSelf();
    return *this;
  }

  template <typename U>
  SharedPtr& operator=(SharedPtr<U>&& other) noexcept {
    DestructorHelper();
    pb_ = reinterpret_cast<BaseControlBlock*>(other.pb_);
    val_ = other.val_;
    other.ForgetYourSelf();
    return *this;
  }

  ~SharedPtr() {
    DestructorHelper();
  }


  size_t use_count() const noexcept {
    return pb_->shared_count_;
  }

  explicit operator bool() const noexcept {
    return use_count() != 0;
  }

  T* operator->() const noexcept {
    return val_;
  }

  T* get() const noexcept {
    return val_;
  }

  T& operator*() const noexcept {
    return *val_;
  }

  void swap(SharedPtr& shr_ptr) noexcept {
    auto tmp = pb_;
    pb_ = shr_ptr.pb_;
    shr_ptr.pb_ = tmp;
    auto val_tmp = val_;
    val_ = shr_ptr.val_;
    shr_ptr.val_ = val_tmp;
  }

  void swap(SharedPtr&& shr_ptr) noexcept {
    DestructorHelper();
    pb_ = shr_ptr.pb_;
    shr_ptr.pb_ = nullptr;
    val_ = shr_ptr.val_;
    shr_ptr.val_ = nullptr;
  }

  void reset() noexcept {
    swap(SharedPtr<T>());
  }

  template <typename U, typename Deleter = std::default_delete<U>, typename Alloc = std::allocator<U>>
  void reset(U* ptr, Deleter del = Deleter(), const Alloc& alloc = Alloc()) noexcept {
    this->swap(SharedPtr<T>(ptr, del, alloc));
  }


  template <typename U>
  friend class SharedPtr;

  template <typename U>
  friend class WeakPtr;

  template <typename U, typename Alloc, typename... Args>
  friend SharedPtr<U> allocateShared(const Alloc& alloc, Args&& ... args);


 private:
  struct BaseControlBlock {
    size_t shared_count_ = 0;
    size_t weak_count_ = 0;

    BaseControlBlock() noexcept  = default;

    void increment_shared_counter() noexcept {
      ++shared_count_;
    };
    void decrement_shared_counter() noexcept {
      --shared_count_;
    };
    void increment_weak_counter() noexcept {
      ++weak_count_;
    };
    void decrement_weak_counter() noexcept {
      --weak_count_;
    };


    virtual T* GetValuePointer() noexcept = 0;
    virtual void DeleteObject() noexcept {};
    virtual void DeleteBlock() noexcept {};
    virtual ~BaseControlBlock() = default;
  };

  template <typename U, typename Alloc = std::allocator<U>>
  struct UnionControlBlock : public BaseControlBlock {
    uint8_t val_[sizeof(U)];
    Alloc alloc_;

    T* GetValuePointer() noexcept override {
      return reinterpret_cast<T*>(val_);
    }

    template <typename... Args>
    UnionControlBlock(Alloc alloc, Args... args) noexcept : alloc_(alloc) {
      std::allocator_traits<Alloc>::construct(alloc, reinterpret_cast<U*>(val_), std::forward<Args>(args)...);
    }

    void DeleteObject() noexcept override {
      std::allocator_traits<Alloc>::destroy(alloc_, reinterpret_cast<U*>(val_));
    }

    ~UnionControlBlock() {};

    void DeleteBlock() noexcept override {
      using new_type = typename SharedPtr<T>::template UnionControlBlock<U, Alloc>;
      using alloc_block_t = typename std::allocator_traits<Alloc>::template rebind_traits<new_type>;
      typename alloc_block_t::allocator_type block_alloc(alloc_);
      this->~UnionControlBlock();
      alloc_block_t::deallocate(block_alloc, this, 1);
    }
  };

  template <typename U, typename Deleter = std::default_delete<U>, typename Alloc = std::allocator<U>>
  struct DividedControlBlock : public BaseControlBlock {
    U* val_;
    Deleter del_;
    Alloc alloc_;

    T* GetValuePointer() noexcept override {
      return val_;
    }

    DividedControlBlock(Deleter del, const Alloc& alloc, U* val) noexcept : val_(val), del_(del),  alloc_(alloc) {}

    void DeleteObject()  noexcept override {
      del_(val_);
    }

     void DeleteBlock() noexcept override {
       using new_type = typename SharedPtr<T>::template DividedControlBlock<U, Deleter, Alloc>;
       using alloc_block_t = typename std::allocator_traits<Alloc>::template rebind_traits<new_type>;
       typename alloc_block_t::allocator_type block_alloc(alloc_);
       this->~DividedControlBlock<U, Deleter, Alloc>();
       alloc_block_t::deallocate(block_alloc, this, 1);
     }

    ~DividedControlBlock() {}
  };

  void DestructorHelper() noexcept {
    if (!pb_) { return; }
    pb_->decrement_shared_counter();
    if (pb_->shared_count_ == 0) {
      pb_->DeleteObject();
      val_ = nullptr;
      if(pb_->weak_count_ == 0) {
        pb_->DeleteBlock();
      }
    }
  }

  void ForgetYourSelf() noexcept {
    pb_ = nullptr;
    val_ = nullptr;
  }

  SharedPtr(BaseControlBlock* pb) noexcept : pb_(pb), val_(pb->GetValuePointer()) {
    if(pb_) pb_->increment_shared_counter();
  }

  BaseControlBlock* pb_;
  T* val_;
};

template <class T, class Alloc, class... Args>
SharedPtr<T> allocateShared(const Alloc& alloc, Args&& ... args) {
  using new_type = typename SharedPtr<T>::template UnionControlBlock<T, Alloc>;
  using alloc_block_t = typename std::allocator_traits<Alloc>::template rebind_traits<new_type>;
  typename alloc_block_t::allocator_type block_alloc(alloc);

  auto block_ptr = alloc_block_t::allocate(block_alloc, 1);
  try {
      new(block_ptr) new_type(alloc, std::forward<Args>(args)...);
  } catch (...) {
    alloc_block_t::deallocate(block_alloc, block_ptr, 1);
  }

  using base_type = typename SharedPtr<T>::BaseControlBlock*;
  return SharedPtr<T>(static_cast<base_type>(block_ptr));
}

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&& ... args) {
  return allocateShared<T, std::allocator<T>, Args...>(std::allocator<T>(), std::forward<Args>(args)...);
}

template <typename T>
class WeakPtr {
 public:
  WeakPtr() = default;

  WeakPtr(const SharedPtr<T>& shr_ptr) noexcept : pb_(shr_ptr.pb_) {
    pb_->increment_weak_counter();
  }

  template <typename U>
  WeakPtr(const SharedPtr<U>& shr_ptr) noexcept : pb_(static_cast<SharedPtr<T>>(shr_ptr).pb_) {
    pb_->increment_weak_counter();
  }

  WeakPtr(SharedPtr<T>&& shr_ptr) noexcept : pb_(shr_ptr.pb_) {
    pb_->increment_weak_counter();
    shr_ptr.DestructorHelper();
    shr_ptr.ForgetYourSelf();
  }

  template <typename U>
  WeakPtr(SharedPtr<U>&& shr_ptr) noexcept : pb_((static_cast<SharedPtr<T>>(shr_ptr)).pb_) {
    pb_->increment_weak_counter();
    shr_ptr.DestructorHelper();
    shr_ptr.ForgetYourSelf();
  }

  WeakPtr& operator=(const SharedPtr<T>& shr_ptr) noexcept {
    DestructorHelper();
    pb_ = shr_ptr.pb_;
    pb_->increment_weak_counter();
    return *this;
  }

  template <typename U>
  WeakPtr& operator=(const SharedPtr<U>& shr_ptr) noexcept {
    DestructorHelper();
    pb_ = static_cast<SharedPtr<T>>(shr_ptr).pb_;
    pb_->increment_weak_counter();
    return *this;
  }

  WeakPtr& operator=(SharedPtr<T>&& shr_ptr) noexcept {
    DestructorHelper();
    pb_ = shr_ptr.pb_;
    pb_->increment_weak_counter();
    shr_ptr.DestructorHelper();
    shr_ptr.ForgetYourSelf();
    return *this;
  }

  template <typename U>
  WeakPtr& operator=(SharedPtr<U>&& shr_ptr) noexcept {
    DestructorHelper();
    pb_ = static_cast<SharedPtr<T>>(shr_ptr).pb_;
    pb_->increment_weak_counter();
    shr_ptr.DestructorHelper();
    shr_ptr.ForgetYourSelf();
    return *this;
  }

  WeakPtr(const WeakPtr<T>& other) noexcept : pb_(other.pb_) {
    pb_->increment_weak_counter();
  }

  template <typename U>
  WeakPtr(const WeakPtr<U>& other) noexcept : WeakPtr(other.lock()) {}

  WeakPtr(WeakPtr<T>&& other) noexcept : pb_(other.pb_) {
    other.ForgetYourSelf();
  }

  template <typename U>
  WeakPtr(WeakPtr<U>&& other) noexcept : WeakPtr(other.lock()) {
    pb_->decrement_weak_counter();
    other.ForgetYourSelf();
  }

  WeakPtr& operator=(const WeakPtr<T>& other) noexcept {
    DestructorHelper();
    pb_ = other.lock().pb_;
    pb_->increment_weak_counter();
    return *this;
  }

  template <typename U>
  WeakPtr& operator=(const WeakPtr<U>& other) noexcept {
    DestructorHelper();
    pb_ = static_cast<SharedPtr<T>>(other.lock()).pb_;
    pb_->increment_weak_counter();
    return *this;
  }

  WeakPtr& operator=(WeakPtr<T>&& other) noexcept {
    DestructorHelper();
    pb_ = other.lock().pb_;
    other.ForgetYourSelf();
    return *this;
  }

  template <typename U>
  WeakPtr& operator=(WeakPtr<U>&& other) noexcept {
    DestructorHelper();
    pb_ = static_cast<SharedPtr<T>>(other.lock()).pb_;
    other.ForgetYourSelf();
    return *this;
  }

  ~WeakPtr() {
    DestructorHelper();
  }


  size_t use_count() const noexcept {
    return pb_->shared_count_;
  }

  bool expired() const noexcept {
    return use_count() == 0;
  }

  SharedPtr<T> lock() const noexcept {
    return expired() ? SharedPtr<T>() : SharedPtr<T>(pb_);
  }


 private:
  void DestructorHelper() noexcept {
    if (!pb_) { return; }
    pb_->decrement_weak_counter();
    if (pb_->shared_count_ == 0 && pb_->weak_count_ == 0) {
      pb_->DeleteBlock();
    }
  }

  void ForgetYourSelf() noexcept {
    pb_ = nullptr;
  }


  template <typename U>
  friend class WeakPtr;


  using BaseControlBlock = typename SharedPtr<T>::BaseControlBlock;
  BaseControlBlock* pb_ = nullptr;
};
