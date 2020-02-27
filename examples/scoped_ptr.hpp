#ifndef EXAMPLES_SCOPED_PTR_HPP_
#define EXAMPLES_SCOPED_PTR_HPP_

template <typename T>
class scoped_ptr final {
public:
  class guard final {
  public:
    ~guard() {
      ptr_->rawptr_ = prev_raw_ptr_;
    }
  private:
    guard(scoped_ptr * ptr, T * prev_raw_ptr)
      : ptr_(ptr), prev_raw_ptr_(prev_raw_ptr)
    {
    }
    friend scoped_ptr;

    scoped_ptr * ptr_;
     T * prev_raw_ptr_;
  };

  scoped_ptr() = default;
  scoped_ptr(scoped_ptr&) = delete;
  scoped_ptr(scoped_ptr&&) = delete;
  scoped_ptr & operator=(scoped_ptr&) = delete;
  scoped_ptr & operator=(scoped_ptr&&) = delete;

  guard reset(T * rawptr) {
    guard g(this, rawptr_);
    rawptr_ = rawptr;
    return g;
  }

  operator bool() const {
    return rawptr_ != nullptr;
  }

  T & operator*() {
    return *rawptr_;
  }

  T * operator->() const {
    return rawptr_;
  }

private:
  T * rawptr_{nullptr};
};

#endif  // EXAMPLES_SCOPED_PTR_HPP_
