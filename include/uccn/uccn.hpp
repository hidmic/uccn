#ifndef UCCN_UCCN_HPP_
#define UCCN_UCCN_HPP_

#include "uccn/uccn.h"

#include <time.h>

#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#if CONFIG_UCCN_MULTITHREADED
#include <thread>
#include <mutex>
#endif

namespace uccn {

class raw_provider final {
 public:
  raw_provider() = default;

  raw_provider(raw_provider &&) = default;
  raw_provider & operator=(raw_provider &&) = default;

  raw_provider(struct uccn_content_provider_s * c_provider)
    : c_provider_(c_provider)
  {
    assert(c_provider != nullptr);
  }

  int post(const buffer_head_s * raw_buffer)
  {
    if (!c_provider_) {
      throw std::logic_error("uninitialized raw content provider");
    }
    int ret = uccn_post(c_provider_, raw_buffer);
    if (ret < 0) {
      std::stringstream message;
      message << "Failed to post content to '"
              << c_provider_->endpoint.resource->path
              << "' resource";
      throw std::runtime_error(message.str());
    }
    return ret;
  }

  template<typename DataT>
  int post(const DataT * data, size_t length)
  {
    buffer_head_s raw_buffer;
    raw_buffer.data = data;
    raw_buffer.size = raw_buffer.length = length;
    return post(&raw_buffer);
  }

  template<typename DataT>
  int post(const std::vector<DataT> buffer)
  {
    buffer_head_s raw_buffer;
    raw_buffer.data = buffer.data();
    raw_buffer.size = buffer.length = buffer.size();
    return post(&raw_buffer);
  }

 private:
  uccn_content_provider_s * c_provider_{nullptr};
};

template<typename ContentT>
class record_provider final
{
public:
  record_provider() = default;

  record_provider(record_provider &&) = default;
  record_provider & operator=(record_provider &&) = default;

  record_provider(struct uccn_content_provider_s * c_provider)
    : c_provider_(c_provider)
  {
    assert(c_provider != nullptr);
  }

  int post(const ContentT * content)
  {
    if (!c_provider_) {
      throw std::logic_error("uninitialized record provider");
    }
    int ret = uccn_post(c_provider_, content);
    if (ret < 0) {
      std::stringstream message;
      message << "Failed to post content to '"
              << c_provider_->endpoint.resource->path
              << "' resource";
      throw std::runtime_error(message.str());
    }
    return ret;
  }

  int post(const ContentT & content)
  {
    return post(&content);
  }

 private:
  uccn_content_provider_s * c_provider_{nullptr};
};

class network final {
 public:
  network(const struct in_addr & inetaddr,
          const struct in_addr & netmask)
  {
    c_network_.inetaddr = inetaddr;
    c_network_.netmask = netmask;
  }

  network(const std::string & inetaddr,
          const std::string & netmask) {
    if (!inet_aton(inetaddr.c_str(), &c_network_.inetaddr)) {
      std::stringstream message;
      message << inetaddr << " is not a valid IP address";
      throw std::runtime_error(message.str());
    }
    if (!inet_aton(netmask.c_str(), &c_network_.netmask)) {
      std::stringstream message;
      message << netmask << " is not a valid IP address";
      throw std::runtime_error(message.str());
    }
  }

  const uccn_network_s * c_network() const noexcept { return &c_network_; }
 private:
  uccn_network_s c_network_;
};

class resource {
 public:
  virtual const uccn_resource_s * c_resource() const noexcept = 0;

  std::string path() const noexcept { return this->c_resource()->path; }
};

class raw_data : public resource {
 public:
  raw_data(const std::string & path) {
    uccn_raw_data_init(&c_raw_data_, path.c_str());
  }

  const uccn_resource_s * c_resource() const noexcept override {
    return &c_raw_data_.base;
  }

 private:
  uccn_raw_data_s c_raw_data_;
};

template<typename ContentT>
class record : public resource {
 public:
  record(const std::string & path)
  {
    uccn_record_init(&c_record_, path.c_str(), ContentT::get_typesupport());
  }

  const uccn_resource_s * c_resource() const noexcept override {
    return &c_record_.base;
  }

 private:
  uccn_record_s c_record_;
};

class node final {
 public:
  node(const network & net, const std::string & name)
  {
    if (uccn_node_init(&c_node_, net.c_network(), name.c_str()) < 0)
    {
      std::stringstream message;
      message << "Failed to initialize '" << name << "' node";
      throw std::runtime_error(message.str());
    }
  }

  ~node()
  {
    if (uccn_node_fini(&c_node_) < 0)
    {
      std::cerr << "Failed to finalize '" << c_node_.name << "' node";
    }
  }

  template<typename ContentT>
  record_provider<ContentT> advertise(const record<ContentT> & resource)
  {
    const uccn_resource_s * c_resource = resource.c_resource();
    uccn_content_provider_s * c_provider = uccn_advertise(&c_node_, c_resource);
    if (c_provider == nullptr) {
      std::stringstream message;
      message << "Failed to advertise '" << c_resource->path << "' resource";
      throw std::runtime_error(message.str());
    }
    return record_provider<ContentT>(c_provider);
  }

  raw_provider advertise(const raw_data & resource)
  {
    const uccn_resource_s * c_resource = resource.c_resource();
    uccn_content_provider_s * c_provider = uccn_advertise(&c_node_, c_resource);
    if (c_provider == nullptr) {
      std::stringstream message;
      message << "Failed to advertise '" << c_resource->path << "' resource";
      throw std::runtime_error(message.str());
    }
    return raw_provider(c_provider);
  }

  template <typename ContentT>
  void track(const record<ContentT> & resource, std::function<void (const ContentT &)> track)
  {
    auto wrapper = [track](void * content) -> void {
      track(*static_cast<ContentT *>(content));
    };
    generic_track(resource, wrapper);
  }

  void track(const raw_data & resource, std::function<void (const buffer_head_s *)> track)
  {
    auto wrapper = [track](void * content) -> void {
      track(static_cast<buffer_head_s *>(content));
    };
    generic_track(resource, wrapper);
  }

  template <typename DataT>
  void track(const raw_data & resource, std::function<void (const DataT *, size_t)> track)
  {
    auto wrapper = [track](void * content) -> void {
      auto raw_buffer = static_cast<buffer_head_s *>(content);
      track(static_cast<DataT *>(raw_buffer->data), raw_buffer->length);
    };
    generic_track(resource, wrapper);
  }

  template <typename DataT>
  void track(const raw_data & resource, std::function<void (const std::vector<DataT>)> track)
  {
    auto wrapper = [track](void * content) -> void {
      auto raw_buffer = static_cast<buffer_head_s *>(content);
      track(std::vector<DataT>(static_cast<DataT *>(raw_buffer->data),
                               static_cast<DataT *>(raw_buffer->data + raw_buffer->length)));
    };
    generic_track(resource, wrapper);
  }

  void spin(const struct timespec * timeout)
  {
    if (uccn_spin(&c_node_, timeout) < 0) {
      std::stringstream message;
      message << "Failed to spin " << c_node_.name << " node";
      throw std::runtime_error(message.str());
    }
  }

  void spin(const std::chrono::nanoseconds & timeout) {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(timeout);
    auto nsecs = timeout - secs;

    struct timespec c_timeout;
    c_timeout.tv_sec = secs.count();
    c_timeout.tv_nsec = nsecs.count();
    spin(&c_timeout);
  }

  void spin() {
    spin(NULL);
  }

  void stop() {
    if (uccn_stop(&c_node_) < 0) {
      std::stringstream message;
      message << "Failed to stop '" << c_node_.name << "' node from spinning";
      throw std::runtime_error(message.str());
    }
  }

 private:
  void generic_track(const resource & resource, std::function<void (void *)> track) {
#if CONFIG_UCCN_MULTITHREADED
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    const uccn_resource_s * c_resource = resource.c_resource();
    generic_track_cpp_functions_[c_resource->hash] = track;
    uccn_content_tracker_s * c_tracker = uccn_track(
        &c_node_, c_resource, &node::generic_track_c_function, this);
    if (c_tracker == NULL) {
      std::stringstream message;
      message << "Failed to track '" << c_resource->path << "' resource";
      throw std::runtime_error(message.str());
    }
  }

  static void generic_track_c_function(uccn_content_tracker_s * tracker, void * content) {
    auto self = static_cast<node *>(tracker->arg);
    uccn_content_endpoint_s * endpoint = &tracker->endpoint;
    self->generic_track_cpp_functions_[endpoint->resource->hash](content);
  };

  std::unordered_map<uint32_t, std::function<void(void *)>> generic_track_cpp_functions_;

  uccn_node_s c_node_;
#if CONFIG_UCCN_MULTITHREADED
  std::mutex mutex_;
#endif
};

}  // namespace uccn

#endif  // UCCN_UCCN_HPP_
