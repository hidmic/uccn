#include "uccn/uccn_internal.h"

#include <assert.h>

#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "uccn/common/hash.h"
#include "uccn/common/logging.h"

static const struct timespec g_uccn_liveliness_timeout = {
  .tv_sec = CONFIG_UCCN_LIVELINESS_TIMEOUT_MS / 1000,
  .tv_nsec = 1000000L * (CONFIG_UCCN_LIVELINESS_TIMEOUT_MS % 1000)
};

static const struct timespec g_uccn_liveliness_assert_timeout = {
  .tv_sec = CONFIG_UCCN_LIVELINESS_ASSERT_TIMEOUT_MS / 1000,
  .tv_nsec = 1000000L * (CONFIG_UCCN_LIVELINESS_ASSERT_TIMEOUT_MS % 1000)
};

static const struct timespec g_uccn_peer_discovery_period = {
  .tv_sec = CONFIG_UCCN_PEER_DISCOVERY_PERIOD_MS / 1000,
  .tv_nsec = 1000000L * (CONFIG_UCCN_PEER_DISCOVERY_PERIOD_MS % 1000),
};

static const struct timespec g_uccn_endpoint_probe_timeout = {
  .tv_sec = CONFIG_UCCN_ENDPOINT_PROBE_TIMEOUT_MS / 1000,
  .tv_nsec = 1000000L * (CONFIG_UCCN_ENDPOINT_PROBE_TIMEOUT_MS % 1000)
};

int uccn_node_init(struct uccn_node_s * node, const struct uccn_network_s * network, const char * name)
{
  int ret, opt = 1;
  struct sockaddr_in address;
  socklen_t address_size;

  assert(node != NULL);
  assert(name != NULL);

  node->socket = node->broadcast_socket = -1;

  node->socket = socket(PF_INET, SOCK_DGRAM, 0);
  if (node->socket < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to create node socket"));
    ret = node->socket;
    goto fail;
  }

  address.sin_family = AF_INET;
  address.sin_port = 0;
  address.sin_addr = network->inetaddr;

  ret = bind(node->socket, (struct sockaddr *)&address, sizeof(address));
  if (ret < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to bind node socket"));
    goto fail;
  }

#ifdef SO_BROADCAST
  ret = setsockopt(node->socket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
  if (ret < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to enable broadcasting for socket"));
    goto fail;
  }
#endif

  address_size = sizeof(node->address);
  if ((ret = getsockname(node->socket, (struct sockaddr *)&node->address, &address_size)) < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to get socket address"));
    goto fail;
  }

  node->broadcast_socket = socket(PF_INET, SOCK_DGRAM, 0);
  if (node->broadcast_socket < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to create node broadcast socket"));
    ret = node->broadcast_socket;
    goto fail;
  }

#ifdef SO_REUSEADDR
  ret = setsockopt(node->broadcast_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  if (ret < 0) {
    uccnwarn(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to reuse address for socket."));
    goto fail;
  }
#endif

  address.sin_family = AF_INET;
  address.sin_port = htons(CONFIG_UCCN_PORT);
  address.sin_addr.s_addr = INADDR_ANY;
  ret = bind(node->broadcast_socket, (struct sockaddr *)&address, sizeof(address));
  if (ret < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to bind node socket"));
    goto fail;
  }

#ifdef SO_BROADCAST
  ret = setsockopt(node->broadcast_socket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
  if (ret < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to enable broadcasting for socket"));
    goto fail;
  }
#endif

  node->broadcast_address.sin_family = AF_INET;
  node->broadcast_address.sin_port = htons(CONFIG_UCCN_PORT);
  node->broadcast_address.sin_addr.s_addr =
      network->inetaddr.s_addr | ~(network->netmaskaddr.s_addr);

  strncpy(node->name, name, CONFIG_UCCN_MAX_NODE_NAME_SIZE);

  inet_ntop(AF_INET, &node->address.sin_addr,
            node->location, sizeof(node->location));
  snprintf(&node->location[strlen(node->location)],
           sizeof(node->location) - strlen(node->location),
           ":%d", ntohs(address.sin_port));

  stack_buffer_init(&node->incoming_buffer, default_storage);
  stack_buffer_init(&node->outgoing_buffer, default_storage);
  stack_buffer_init(&node->content_buffer, default_storage);

  memset(node->peers, 0, sizeof(node->peers));
  memset(node->trackers, 0, sizeof(node->trackers));
  memset(node->providers, 0, sizeof(node->providers));
  node->num_peers = node->num_providers = node->num_trackers = 0;

#if CONFIG_UCCN_MULTITHREADED
  ret = pthread_mutex_init(&node->mutex, NULL);
  if (ret < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to initialize node mutex"));
    goto fail;
  }
#endif

  ret = eventfd_init(&node->stop_event);
  if (ret < 0) {
    uccndbg(BACKTRACE_FROM(__LINE__ - 2));
    goto fail;
  }

  return 0;
fail:
  if (node->socket >= 0 && close(node->socket) < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to close node socket"));
  }
  if (node->broadcast_socket >= 0 && close(node->broadcast_socket) < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to close node broadcast socket"));
  }
  return ret;
}

struct uccn_content_tracker_s *
uccn_track(struct uccn_node_s * node,
           const struct uccn_resource_s * resource,
           const uccn_content_track_fn track, void * arg)
{
  size_t i;

  struct uccn_content_endpoint_s * endpoint;
  struct uccn_content_tracker_s * tracker;

#if CONFIG_UCCN_MULTITHREADED
  if (pthread_mutex_lock(&node->mutex) < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to lock node mutex"));
    return NULL;
  }
#endif
  for (i = 0; i < node->num_trackers; ++i) {
    tracker = &node->trackers[i];
    endpoint = (struct uccn_content_endpoint_s *)tracker;
    if (endpoint->resource == resource) {
      uccndbg("Updating existing tracker for '%s' resource", resource->path);
      tracker->track = track;
      tracker->arg = arg;
      goto leave;
    }
    if (endpoint->resource->hash == resource->hash) {
      uccnerr(RUNTIME_ERR("'%s' resource hash collides with '%s's",
                          resource->path, endpoint->resource->path));
      tracker = NULL;
      goto leave;
    }
  }
  tracker = &node->trackers[node->num_trackers++];
  endpoint = (struct uccn_content_endpoint_s *)tracker;
  endpoint->node = node;
  endpoint->resource = resource;
  endpoint->num_peers = 0;
  tracker->track = track;
  tracker->arg = arg;
 leave:
#if CONFIG_UCCN_MULTITHREADED
  assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif
  return tracker;
}

struct uccn_content_provider_s *
uccn_advertise(struct uccn_node_s * node, const struct uccn_resource_s * resource)
{
  size_t i;
  struct uccn_content_provider_s * provider;
  struct uccn_content_endpoint_s * endpoint;

#if CONFIG_UCCN_MULTITHREADED
  if (pthread_mutex_lock(&node->mutex) < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to lock node mutex"));
    return NULL;
  }
#endif
  for (i = 0; i < node->num_providers; ++i) {
    provider = &node->providers[i];
    endpoint = (struct uccn_content_endpoint_s *)provider;

    if (endpoint->resource == resource) {
      uccndbg("Provider for '%s' resource already registered", resource->path);
      return provider;
    }
    if (endpoint->resource->hash == resource->hash) {
      uccnerr(RUNTIME_ERR("'%s' resource hash collides with '%s's",
                          resource->path, endpoint->resource->path));
      return NULL;
    }
  }
  provider = &node->providers[node->num_providers++];
  endpoint = (struct uccn_content_endpoint_s *)provider;
  endpoint->node = node;
  endpoint->resource = resource;
  endpoint->num_peers = 0;
#if CONFIG_UCCN_MULTITHREADED
  assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif
  return provider;
}

int uccn_post(struct uccn_content_provider_s * provider, const void * content)
{
  int ret;
  size_t i;
  ssize_t nbytes;

  struct buffer_head_s * blob;
  struct buffer_head_s * packet;

  mpack_error_t err;
  mpack_writer_t writer;

  struct uccn_peer_s * peer;
  struct timespec new_liveliness_deadline;

  struct uccn_content_endpoint_s * endpoint =
      (struct uccn_content_endpoint_s *)provider;
  struct uccn_node_s * node = endpoint->node;
  const struct uccn_resource_s * resource = endpoint->resource;

  ret = 0;
  if (endpoint->num_peers > 0) {
    if ((ret = clock_gettime(CLOCK_MONOTONIC, &new_liveliness_deadline)) != 0) {
      uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to get current time"));
      return ret;
    }
    timespec_add(&new_liveliness_deadline, &g_uccn_liveliness_assert_timeout);
#if CONFIG_UCCN_MULTITHREADED
    ret = pthread_mutex_lock(&node->mutex);
    if (ret < 0) {
      uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to lock node mutex"));
      return ret;
    }
#endif
    packet = (struct buffer_head_s *)&node->outgoing_buffer;
    mpack_writer_init(&writer, packet->data, packet->size);
    {
      mpack_start_map(&writer, 1);
      {
        mpack_write_u8(&writer, UCCN_CONTENT_GROUP);
        mpack_start_map(&writer, 1);
        {
          mpack_write_u32(&writer, resource->hash);
          blob = (struct buffer_head_s *)&node->content_buffer;
          if ((ret = resource->pack(resource, content, &blob)) < 0) {
            uccndbg(BACKTRACE_FROM(__LINE__ - 1));
#if CONFIG_UCCN_MULTITHREADED
            assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif
            return ret;
          }
          mpack_write_bin(&writer, blob->data, blob->length);
        }
        mpack_finish_map(&writer);
      }
      mpack_finish_map(&writer);
    }
    packet->length = mpack_writer_buffer_used(&writer);
    if ((err = mpack_writer_destroy(&writer)) != mpack_ok) {
      uccnerr(RUNTIME_ERR("Failed to build '%s' content packet: %s",
                          resource->path, mpack_error_to_string(err)));
#if CONFIG_UCCN_MULTITHREADED
      assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif
      return -1;
    }

    for (ret = 0, i = 0; i < endpoint->num_peers; ++i) {
      peer = endpoint->peers[i];

      nbytes = sendto(node->socket, packet->data, packet->length, 0,
                      (struct sockaddr *)&peer->address,
                      sizeof(peer->address));
      assert(nbytes < 0 || (size_t)nbytes == packet->length);
      if (nbytes < 0) {
        uccnwarn(SYSTEM_ERR_FROM(__LINE__ - 3, "Failed to send '%s' content", resource->path));
        continue;
      }
      peer->liveliness.next_local_deadline = new_liveliness_deadline;
      ++ret;
    }
#if CONFIG_UCCN_MULTITHREADED
    assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif
  }

  return ret;
}

struct uccn_peer_s *
uccn_register_peer(struct uccn_node_s * node, struct sockaddr_in * address)
{
  size_t i;
  struct timespec current_time;
  struct uccn_peer_s * peer;

  assert(node != NULL);
  assert(address != NULL);

  if (node->num_peers >= CONFIG_UCCN_MAX_NUM_PEERS) {
    uccnerr(RUNTIME_ERR("Too many peers, ignoring"));
    return NULL;
  }

  if (clock_gettime(CLOCK_MONOTONIC, &current_time) < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to get current time"));
    return NULL;
  }

  for (i = 0; i < node->num_peers; ++i) {
    peer = &node->peers[i];

    if (same_sockaddr_in(&peer->address, address)) {
      peer->liveliness.next_remote_deadline = current_time;
      timespec_add(&peer->liveliness.next_remote_deadline, &g_uccn_liveliness_timeout);
      return peer;
    }
  }

  peer = &node->peers[node->num_peers++];
  peer->address = *address;

  strncpy(peer->name, "anon", CONFIG_UCCN_MAX_NODE_NAME_SIZE);
  inet_ntop(AF_INET, &address->sin_addr, peer->location, sizeof(peer->location));
  snprintf(&peer->location[strlen(peer->location)],
           sizeof(peer->location) - strlen(peer->location),
           ":%d", ntohs(address->sin_port));

  peer->alive = true;
  peer->liveliness.next_local_deadline = current_time;
  peer->liveliness.next_remote_deadline = current_time;
  timespec_add(&peer->liveliness.next_remote_deadline, &g_uccn_liveliness_timeout);
  peer->num_links = 0;
  uccndbg("Peer %s@%s registered", peer->name, peer->location);
  return peer;
}

static ssize_t content_passthrough(struct uccn_resource_s * resource,
                                   struct buffer_head_s * input,
                                   struct buffer_head_s ** output) {
  (void)resource;
  assert(input != NULL);
  assert(output != NULL);

  *output = input;
  return input->length;
}

void uccn_resource_init(struct uccn_resource_s * resource, const char * path)
{
  assert(resource != NULL);
  assert(path != NULL);
  strncpy(resource->path, path, CONFIG_UCCN_MAX_RESOURCE_PATH_SIZE);
  resource->hash = djb2(path);
  resource->pack = (uccn_content_pack_fn)content_passthrough;
  resource->unpack = (uccn_content_unpack_fn)content_passthrough;
}

static ssize_t generic_record_pack(const struct uccn_record_s * record,
                                   const void * content,
                                   struct buffer_head_s ** blob)
{
  assert(record != NULL);
  assert(content != NULL);
  assert(blob != NULL);

  if (*blob == NULL) {
    uccnerr(RUNTIME_ERR("Need a blob to pack content into"));
    return -1;
  }
  return record->ts->serialize(record->ts, content, *blob);
}

static ssize_t generic_record_unpack(const struct uccn_record_s * record,
                                     const struct buffer_head_s * blob,
                                     void ** content)
{
  assert(record != NULL);
  assert(blob != NULL);
  assert(content != NULL);

  if (*content == NULL) {
    if ((*content = record->ts->allocate(record->ts)) == NULL) {
      uccndbg(BACKTRACE_FROM(__LINE__ - 1));
      return -1;
    }
  }
  return record->ts->deserialize(record->ts, blob, *content);
}

void uccn_record_init(struct uccn_record_s * record, const char * path,
                      const struct uccn_record_typesupport_s * ts)
{
  assert(record != NULL);
  assert(path != NULL);
  assert(ts != NULL);
  assert(ts->serialize != NULL);
  assert(ts->deserialize != NULL);
  uccn_resource_init((struct uccn_resource_s *)record, path);
  record->base.pack = (uccn_content_pack_fn)generic_record_pack;
  record->base.unpack = (uccn_content_unpack_fn)generic_record_unpack;
  record->ts = ts;
}

int uccn_prepare_keepalive_packet(struct uccn_node_s * node, struct buffer_head_s * packet)
{
  mpack_error_t err;
  mpack_writer_t writer;

  (void)node;
  assert(packet != NULL);
  assert(packet->data != NULL);

  mpack_writer_init(&writer, packet->data, packet->size);
  mpack_write_nil(&writer);
  packet->length = mpack_writer_buffer_used(&writer);
  if ((err = mpack_writer_destroy(&writer)) != mpack_ok) {
    uccnerr(RUNTIME_ERR("Failed to build keepalive packet: %s",
                        mpack_error_to_string(err)));
    return -1;
  }
  return 0;
}

int uccn_prepare_discovery_packet(struct uccn_node_s * node, struct buffer_head_s * packet)
{
  size_t i;

  mpack_error_t err;
  mpack_writer_t writer;

  uint32_t num_hashes = 0;
  uint32_t hashes[CONFIG_UCCN_MAX_NUM_TRACKERS];

  struct uccn_content_endpoint_s * endpoint;

  assert(node != NULL);
  assert(packet != NULL);
  assert(packet->data != NULL);

  mpack_writer_init(&writer, packet->data, packet->size);
  mpack_start_map(&writer, 1);
  {
    mpack_write_u8(&writer, UCCN_LINK_GROUP);
    mpack_start_map(&writer, 2);
    {
      mpack_write_u8(&writer, UCCN_NODE_NAME);
      mpack_write_cstr(&writer, node->name);
    }
    {
      mpack_write_u8(&writer, UCCN_TRACKED_ARRAY);
      for (i = 0; i < node->num_trackers; ++i) {
        endpoint = (struct uccn_content_endpoint_s *)&node->trackers[i];
        if (endpoint->num_peers == 0) {
          hashes[num_hashes++] = endpoint->resource->hash;
        }
      }
      mpack_start_array(&writer, num_hashes);
      {
        for (i = 0; i < num_hashes; ++i) {
          mpack_write_u32(&writer, hashes[i]);
        }
      }
      mpack_finish_array(&writer);
    }
    mpack_finish_map(&writer);
  }
  mpack_finish_map(&writer);
  packet->length = mpack_writer_buffer_used(&writer);
  if ((err = mpack_writer_destroy(&writer)) != mpack_ok) {
    uccnerr(RUNTIME_ERR("Failed to build discovery packet: %s",
                        mpack_error_to_string(err)));
    return -1;
  }
  return 0;
}

int uccn_assert_liveliness(struct uccn_node_s * node, struct timespec * next_deadline)
{
  int ret;
  size_t i;
  ssize_t nbytes;
  struct timespec current_time;
  struct uccn_peer_s * peer;
  struct buffer_head_s * outgoing_packet;

  if (next_deadline != NULL) {
    TIMESPEC_INF_INIT(next_deadline);
  }

  outgoing_packet = (struct buffer_head_s *)&node->outgoing_buffer;
  if ((ret = uccn_prepare_keepalive_packet(node, outgoing_packet)) < 0) {
    uccndbg(BACKTRACE_FROM(__LINE__ - 1));
    return ret;
  }

  if ((ret = clock_gettime(CLOCK_MONOTONIC, &current_time)) != 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to get current time"));
    return ret;
  }

  for (i = 0; i < node->num_peers; ++i) {
    peer = &node->peers[i];

    if (timespec_cmp(&peer->liveliness.next_local_deadline, &current_time) <= 0) {
      nbytes = sendto(node->socket, outgoing_packet->data, outgoing_packet->length,
                      0, (struct sockaddr *)&peer->address,
                      sizeof(peer->address));
      assert(nbytes < 0 || (size_t)nbytes == outgoing_packet->length);
      if (nbytes < 0) {
        uccnerr(SYSTEM_ERR_FROM(__LINE__ - 3, "Failed to send keepalive packet"));
        ret = nbytes;
        break;
      }
      timespec_add(&peer->liveliness.next_local_deadline, &g_uccn_liveliness_assert_timeout);
    }

    if (next_deadline != NULL) {
      if (timespec_cmp(next_deadline, &peer->liveliness.next_local_deadline) > 0) {
        *next_deadline = peer->liveliness.next_local_deadline;
      }
    }
  }

  return ret;
}


int uccn_discover_peers(struct uccn_node_s * node)
{
  int ret = 0;
  ssize_t nbytes;
  struct buffer_head_s * outgoing_packet;

  outgoing_packet = (struct buffer_head_s *)&node->outgoing_buffer;
  if ((ret = uccn_prepare_discovery_packet(node, outgoing_packet)) < 0) {
    uccndbg(BACKTRACE_FROM(__LINE__ - 1));
    return ret;
  }

  uccndbg("Attempting to discover peers");
  nbytes = sendto(node->socket, outgoing_packet->data, outgoing_packet->length,
                  0, (struct sockaddr *)&node->broadcast_address,
                  sizeof(node->broadcast_address));
  assert(nbytes < 0 || (size_t)nbytes == outgoing_packet->length);
  if (nbytes < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 3, "Failed to send discovery packet"));
    ret = nbytes;
  }

  return ret;
}

int uccn_process_incoming_unicast(struct uccn_node_s * node)
{
  int ret;
  ssize_t nbytes;
  struct sockaddr_in address;
  socklen_t address_size = sizeof(address);
  struct buffer_head_s * incoming_packet;

  assert(node != NULL);

  incoming_packet = (struct buffer_head_s *)&node->incoming_buffer;
  nbytes = recvfrom(
      node->socket, incoming_packet->data, incoming_packet->size,
      MSG_DONTWAIT, (struct sockaddr *)&address, &address_size);
  if (nbytes < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to receive packet"));
    return nbytes;
  }
  incoming_packet->length = nbytes;

  if (same_sockaddr_in(&address, &node->address)) {
    uccnerr(RUNTIME_ERR("Port reuse is not supported"));
    return -1;
  }

  if ((ret = uccn_process_incoming(node, &address, incoming_packet)) < 0) {
    uccndbg(BACKTRACE_FROM(__LINE__ - 1));
  }
  return ret;
}

int uccn_process_incoming_broadcast(struct uccn_node_s * node)
{
  int ret;
  ssize_t nbytes;
  struct sockaddr_in address;
  socklen_t address_size = sizeof(address);
  struct buffer_head_s * incoming_packet;

  assert(node != NULL);

  incoming_packet = (struct buffer_head_s *)&node->incoming_buffer;
  nbytes = recvfrom(
      node->broadcast_socket, incoming_packet->data, incoming_packet->size,
      MSG_DONTWAIT, (struct sockaddr *)&address, &address_size);
  if (nbytes < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to receive packet"));
    return nbytes;
  }
  incoming_packet->length = nbytes;

  if (same_sockaddr_in(&address, &node->address)) {
    // Ignoring broadcast to self
    return 0;
  }

  if ((ret = uccn_process_incoming(node, &address, incoming_packet)) < 0) {
    uccndbg(BACKTRACE_FROM(__LINE__ - 1));
  }
  return ret;
}


int uccn_process_incoming(struct uccn_node_s * node,
                          struct sockaddr_in * origin,
                          struct buffer_head_s * incoming_packet)
{
  int ret;
  ssize_t nbytes;
  struct uccn_peer_s * peer;
  struct buffer_head_s * outgoing_packet;

  if ((peer = uccn_register_peer(node, origin)) == NULL) {
    uccndbg(BACKTRACE_FROM(__LINE__ - 1));
    return -1;
  }

  outgoing_packet = (struct buffer_head_s *)&node->outgoing_buffer;
  ret = uccn_process_packet(node, peer, incoming_packet, outgoing_packet);
  if (ret < 0) {
    uccndbg(BACKTRACE_FROM(__LINE__ - 2));
    return ret;
  }

  if (outgoing_packet->length > 0) {
    nbytes = sendto(
        node->socket, outgoing_packet->data, outgoing_packet->length, 0,
        (struct sockaddr *)&peer->address, sizeof(peer->address));
    assert(nbytes < 0 || (size_t)nbytes == outgoing_packet->length);
    if (nbytes < 0) {
      uccnerr(SYSTEM_ERR_FROM(__LINE__ - 3, "Failed to send packet"));
      return nbytes;
    }
  }

  return 0;
}

int uccn_spin(struct uccn_node_s * node, const struct timespec * timeout)
{
  fd_set rfds;
  int nfds, nret = 0, ret = 0;
  size_t num_active_trackers;
  struct timespec stimeout;
  struct timespec current_time;
  struct timespec next_deadline;
  struct timespec next_probe_time;
  struct timespec next_assert_time;
  struct timespec next_discovery_time;
  struct timespec timeout_time = TIMESPEC_INF;

  assert(node != NULL);

  if ((ret = clock_gettime(CLOCK_MONOTONIC, &current_time)) != 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to get current time"));
    return ret;
  }
  next_assert_time = current_time;
  next_discovery_time = current_time;
  next_probe_time = current_time;

  if (timeout) {
    timeout_time = current_time;
    timespec_add(&timeout_time, timeout);
  }

  FD_ZERO(&rfds);
  nfds = eventfd_fileno(&node->stop_event);
  if (node->broadcast_socket > nfds) {
    nfds = node->broadcast_socket;
  }
  if (node->socket > nfds) {
    nfds = node->socket;
  }
  nfds += 1;

  do {
#if CONFIG_UCCN_MULTITHREADED
    ret = pthread_mutex_lock(&node->mutex);
    if (ret < 0) {
      uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to lock node mutex"));
      break;
    }
#endif
    if (nret > 0) {
      if (FD_ISSET(eventfd_fileno(&node->stop_event), &rfds)) {
        ret = eventfd_clear(&node->stop_event);
        if (ret < 0) {
          uccndbg(BACKTRACE_FROM(__LINE__ - 2));
#if CONFIG_UCCN_MULTITHREADED
          assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif
        }
        break;
      }
      if (FD_ISSET(node->socket, &rfds)) {
        if ((ret = uccn_process_incoming_unicast(node)) < 0) {
          uccndbg(BACKTRACE_FROM(__LINE__ - 1));
#if CONFIG_UCCN_MULTITHREADED
          assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif
          break;
        }
      }
      if (FD_ISSET(node->broadcast_socket, &rfds)) {
        if ((ret = uccn_process_incoming_broadcast(node)) < 0) {
          uccndbg(BACKTRACE_FROM(__LINE__ - 1));
#if CONFIG_UCCN_MULTITHREADED
          assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif
          break;
        }
      }
    }

    if ((ret = clock_gettime(CLOCK_MONOTONIC, &current_time)) != 0) {
      uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to get current time"));
#if CONFIG_UCCN_MULTITHREADED
      assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif
      break;
    }

    if (timespec_cmp(&current_time, &next_assert_time) >= 0) {
      if ((ret = uccn_assert_liveliness(node, &next_assert_time)) < 0) {
        uccndbg(BACKTRACE_FROM(__LINE__ - 1));
#if CONFIG_UCCN_MULTITHREADED
        assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif
        break;
      }
    }

    if (timespec_cmp(&current_time, &next_probe_time) >= 0) {
      if ((ret = uccn_probe_endpoints(node, &num_active_trackers,
                                      NULL, &next_probe_time)) < 0) {
        uccndbg(BACKTRACE_FROM(__LINE__ - 2));
#if CONFIG_UCCN_MULTITHREADED
        assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif
        break;
      }
    }

    if (num_active_trackers < node->num_trackers) {
      if (timespec_cmp(&current_time, &next_discovery_time) >= 0) {
        if ((ret = uccn_discover_peers(node)) < 0) {
          uccndbg(BACKTRACE_FROM(__LINE__ - 1));
#if CONFIG_UCCN_MULTITHREADED
          assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif
          break;
        }
        timespec_add(&next_discovery_time, &g_uccn_peer_discovery_period);
      }
    }
#if CONFIG_UCCN_MULTITHREADED
    assert(pthread_mutex_unlock(&node->mutex) == 0);
#endif

    if ((ret = clock_gettime(CLOCK_MONOTONIC, &current_time)) != 0) {
      uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to get current time"));
      break;
    }

    if (timespec_cmp(&current_time, &timeout_time) >= 0) {
      break;
    }
    next_deadline = timeout_time;
    if (timespec_cmp(&next_deadline, &next_assert_time) > 0) {
      next_deadline = next_assert_time;
    }
    if (timespec_cmp(&next_deadline, &next_probe_time) > 0) {
      next_deadline = next_probe_time;
    }
    if (timespec_cmp(&next_deadline, &next_discovery_time) > 0) {
      next_deadline = next_discovery_time;
    }
    assert(TIMESPEC_ISFINITE(&next_deadline));

    do {
      FD_ZERO(&rfds);
      FD_SET(node->socket, &rfds);
      FD_SET(node->broadcast_socket, &rfds);
      FD_SET(eventfd_fileno(&node->stop_event), &rfds);

      if ((ret = clock_gettime(CLOCK_MONOTONIC, &current_time)) != 0) {
        uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to get current time"));
        break;
      }

      stimeout = next_deadline;
      timespec_diff(&stimeout, &current_time);
      ret = nret = pselect(nfds, &rfds, NULL, NULL, &stimeout, NULL);
    } while(ret < 0 && errno == EINTR);

    if (ret < 0) {
      uccnerr(SYSTEM_ERR_FROM(__LINE__ - 3, "Failed to poll sockets"));
    }
  } while (ret >= 0);

  return ret;
}

void uccn_unlink_dead_peers(struct uccn_content_endpoint_s * endpoint) {
  size_t i, j;

  assert(endpoint != NULL);

  for (i = 0; i < endpoint->num_peers; ++i) {
    if (!endpoint->peers[i]->alive) {
      for (j = i; j < endpoint->num_peers - 1; ++j) {
        endpoint->peers[j] = endpoint->peers[j + 1];
      }
      --endpoint->num_peers, --i;
    }
  }
}

int uccn_probe_endpoints(struct uccn_node_s * node,
                         size_t * num_active_trackers,
                         size_t * num_active_providers,
                         struct timespec * next_probe_time)
{
  int ret;
  size_t i, j;
  struct timespec current_time;

  struct uccn_peer_s * peer;
  struct uccn_content_endpoint_s * endpoint;
  struct timespec * peer_liveliness_deadline;

  assert(node != NULL);

  if (num_active_trackers != NULL) {
    *num_active_trackers = 0;
  }
  if (num_active_providers != NULL) {
    *num_active_providers = 0;
  }

  if ((ret = clock_gettime(CLOCK_MONOTONIC, &current_time)) != 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 1, "Failed to get current time"));
    return ret;
  }

  for (i = 0; i < node->num_peers; ++i) {
    peer = &node->peers[i];

    peer_liveliness_deadline = &peer->liveliness.next_remote_deadline;
    peer->alive = (timespec_cmp(peer_liveliness_deadline, &current_time) >= 0);
  }

  for (i = 0; i < node->num_trackers; ++i) {
    endpoint = (struct uccn_content_endpoint_s *)&node->trackers[i];

    uccn_unlink_dead_peers(endpoint);

    if (num_active_trackers != NULL) {
      if (endpoint->num_peers > 0) {
        *num_active_trackers += 1;
      }
    }
  }

  for (i = 0; i < node->num_providers; ++i) {
    endpoint = (struct uccn_content_endpoint_s *)&node->providers[i];

    uccn_unlink_dead_peers(endpoint);

    if (num_active_providers != NULL) {
      if (endpoint->num_peers > 0) {
        *num_active_providers += 1;
      }
    }
  }

  if (next_probe_time != NULL) {
    *next_probe_time = current_time;
    timespec_add(next_probe_time, &g_uccn_endpoint_probe_timeout);
  }

  for (i = 0; i < node->num_peers; ++i) {
    peer = &node->peers[i];

    if (!peer->alive || peer->num_links == 0) {
      for (j = i; j < node->num_peers - 1; ++j) {
        node->peers[j] = node->peers[j + 1];
      }
      --node->num_peers, --i;
      continue;
    }

    peer_liveliness_deadline = &peer->liveliness.next_remote_deadline;
    if (next_probe_time != NULL) {
      if (timespec_cmp(next_probe_time, peer_liveliness_deadline) > 0) {
        *next_probe_time = *peer_liveliness_deadline;
      }
    }
  }

  return ret;
}

int uccn_process_content_blob(struct uccn_node_s * node, struct uccn_peer_s * peer,
                              uint32_t hash, struct buffer_head_s * blob)
{
  int ret = 0;

  size_t i;
  void * content;
  struct uccn_content_tracker_s * tracker;
  struct uccn_content_endpoint_s * endpoint;

  for (i = 0; i < node->num_trackers; ++i) {
    tracker = &node->trackers[i];
    endpoint = (struct uccn_content_endpoint_s *)tracker;

    if (endpoint->resource->hash == hash) {

      content = NULL;
      if ((ret = endpoint->resource->unpack(endpoint->resource, blob, &content)) < 0) {
        uccndbg(BACKTRACE_FROM(__LINE__ - 1));
        break;
      }

      tracker->track(tracker, content);

      if ((ret = uccn_link(endpoint, peer)) != 0) {
        uccndbg(BACKTRACE_FROM(__LINE__ - 1));
      }
      break;
    }
  }

  return ret;
}

int uccn_process_content_group(struct uccn_node_s * node, struct uccn_peer_s * peer, mpack_reader_t * reader)
{
  int ret = 0;
  uint32_t hash;
  uint32_t i, group_size;
  struct buffer_head_s blob;

  assert(node != NULL);
  assert(peer != NULL);
  assert(reader != NULL);

  if (mpack_expect_map_or_nil(reader, &group_size)) {
    for (i = 0; i < group_size; ++i) {
      hash = mpack_expect_u32(reader);
      if (hash == 0) {
        uccnerr(RUNTIME_ERR("Content hash missing"));
        ret = -1;
        break;
      }
      blob.length = blob.size = mpack_expect_bin(reader);
      if (blob.length == 0) {
        uccnerr(RUNTIME_ERR("Content blob missing"));
        ret = -1;
        break;
      }
      blob.data = (void *)mpack_read_bytes_inplace(reader, blob.length);
      if (blob.data == NULL) {
        uccnerr(RUNTIME_ERR("Failed to read content blob inplace"));
        ret = -1;
        break;
      }
      ret = uccn_process_content_blob(node, peer, hash, &blob);
      if (ret != 0) {
        uccndbg(BACKTRACE_FROM(__LINE__ - 2));
        break;
      }
    }
    mpack_done_map(reader);
  }

  return ret;
}

int uccn_link(struct uccn_content_endpoint_s * endpoint, struct uccn_peer_s * peer)
{
  size_t i;

  if (endpoint->num_peers >= CONFIG_UCCN_MAX_NUM_PEERS) {
    uccnerr(RUNTIME_ERR("Too many peers"));
    return -1;
  }

  for (i = 0; i < endpoint->num_peers; ++i) {
    if (endpoint->peers[i] == peer) {
      return 0;
    }
  }
  endpoint->peers[endpoint->num_peers++] = peer;
  ++peer->num_links;
  return 1;
}

int uccn_unlink(struct uccn_content_endpoint_s * endpoint, struct uccn_peer_s * peer)
{
  size_t i, j;

  for (i = 0; i < endpoint->num_peers; ++i) {
    if (endpoint->peers[i] == peer) {
      for (j = i; j < endpoint->num_peers - 1; ++j) {
        endpoint->peers[j] = endpoint->peers[j + 1];
      }
      --endpoint->num_peers;
      --peer->num_links;
      return 1;
    }
  }
  return 0;
}

int uccn_link_trackers(struct uccn_node_s * node, struct uccn_peer_s * peer, uint32_t hash)
{
  int ret, num_links = 0;

  size_t i;
  struct uccn_content_endpoint_s * endpoint;
  for (i = 0; i < node->num_trackers; ++i) {
    endpoint = (struct uccn_content_endpoint_s *)&node->trackers[i];
    if (endpoint->resource->hash == hash) {
      if ((ret = uccn_link(endpoint, peer)) < 0) {
        uccndbg(BACKTRACE_FROM(__LINE__ - 1));
        return ret;
      }
      num_links += ret;
    }
  }
  return num_links;
}

int uccn_link_providers(struct uccn_node_s * node, struct uccn_peer_s * peer, uint32_t hash)
{
  int ret, num_links = 0;

  size_t i;
  struct uccn_content_endpoint_s * endpoint;
  for (i = 0; i < node->num_providers; ++i) {
    endpoint = (struct uccn_content_endpoint_s *)&node->providers[i];
    if (endpoint->resource->hash == hash) {
      if ((ret = uccn_link(endpoint, peer)) < 0) {
        uccndbg(BACKTRACE_FROM(__LINE__ - 1));
        return ret;
      }
      num_links += ret;
    }
  }
  return num_links;
}

int uccn_unlink_providers(struct uccn_node_s * node, struct uccn_peer_s * peer)
{
  size_t i;
  int num_unlinks = 0;
  struct uccn_content_endpoint_s * endpoint;
  for (i = 0; i < node->num_providers; ++i) {
    endpoint = (struct uccn_content_endpoint_s *)&node->providers[i];
    num_unlinks += uccn_unlink(endpoint, peer);
  }
  return num_unlinks;
}

int uccn_unlink_trackers(struct uccn_node_s * node, struct uccn_peer_s * peer)
{
  size_t i;
  int num_unlinks = 0;
  struct uccn_content_endpoint_s * endpoint;
  for (i = 0; i < node->num_trackers; ++i) {
    endpoint = (struct uccn_content_endpoint_s *)&node->trackers[i];
    num_unlinks += uccn_unlink(endpoint, peer);
  }
  return num_unlinks;
}

int uccn_process_link_group(struct uccn_node_s * node, struct uccn_peer_s * peer,
                            mpack_reader_t * reader, mpack_writer_t * writer)
{
  int ret = 0;

  uint32_t i, j;

  uint32_t hash;
  uint8_t  data_code;
  uint32_t group_size, array_size;

  uint32_t provided_hashes[CONFIG_UCCN_MAX_NUM_RESOURCES];
  uint32_t num_provided_hashes = 0;

  uint32_t tracked_hashes[CONFIG_UCCN_MAX_NUM_RESOURCES];
  uint32_t num_tracked_hashes = 0;

  assert(node != NULL);
  assert(peer != NULL);
  assert(reader != NULL);
  assert(writer != NULL);

  if (mpack_expect_map_max_or_nil(reader, UCCN_MAX_NUM_LINK_DATA, &group_size)) {
    for (i = 0; i < group_size; ++i) {
      data_code = mpack_expect_u8(reader);
      switch(data_code) {
        case UCCN_NODE_NAME:
          mpack_expect_cstr(reader, peer->name, CONFIG_UCCN_MAX_NODE_NAME_SIZE);
          break;
        case UCCN_PROVIDED_ARRAY:
          if (mpack_expect_array_max_or_nil(reader, CONFIG_UCCN_MAX_NUM_RESOURCES, &array_size)) {
            if ((ret = uccn_unlink_trackers(node, peer)) < 0) {
              uccndbg(BACKTRACE_FROM(__LINE__ - 1));
              return ret;
            }
            for (j = 0; j < array_size; ++j) {
              hash = mpack_expect_u32(reader);
              if (hash == 0) {
                uccnerr(RUNTIME_ERR("Missing resource hash"));
                return -1;
              }
              if ((ret = uccn_link_trackers(node, peer, hash)) < 0) {
                uccndbg(BACKTRACE_FROM(__LINE__ -1));
                return ret;
              }
              if (ret > 0) {
                tracked_hashes[num_tracked_hashes++] = hash;
              }
            }
            mpack_done_array(reader);
          }
          break;
        case UCCN_TRACKED_ARRAY:
          if ((ret = uccn_unlink_providers(node, peer)) < 0) {
            uccndbg(BACKTRACE_FROM(__LINE__ - 1));
            return ret;
          }
          if (mpack_expect_array_max_or_nil(reader, CONFIG_UCCN_MAX_NUM_RESOURCES, &array_size)) {
            for (j = 0; j < array_size; ++j) {
              hash = mpack_expect_u32(reader);
              if (hash == 0) {
                uccnerr(RUNTIME_ERR("Missing resource hash"));
                return -1;
              }
              if ((ret = uccn_link_providers(node, peer, hash)) < 0) {
                uccndbg(BACKTRACE_FROM(__LINE__ -1));
                return ret;
              }
              if (ret > 0) {
                provided_hashes[num_provided_hashes++] = hash;
              }
            }
            mpack_done_array(reader);
          }
          break;
        default:
          uccnerr(RUNTIME_ERR("Unknown link group data code: %hhu", data_code));
          return -1;
      }
    }
    mpack_done_map(reader);
  }
  group_size = 0;
  if (num_tracked_hashes > 0) {
    group_size += 1;
  }
  if (num_provided_hashes > 0) {
    group_size += 1;
  }
  if (group_size > 0) {
    mpack_start_map(writer, 1);
    {
      mpack_write_u8(writer, UCCN_LINK_GROUP);
      mpack_start_map(writer, group_size + 1);
      {
        mpack_write_u8(writer, UCCN_NODE_NAME);
        mpack_write_cstr(writer, node->name);
        if (num_tracked_hashes > 0) {
          mpack_write_u8(writer, UCCN_TRACKED_ARRAY);
          mpack_start_array(writer, num_tracked_hashes);
          while (num_tracked_hashes > 0) {
            mpack_write_u32(writer, tracked_hashes[--num_tracked_hashes]);
          }
          mpack_finish_array(writer);
        }
        if (num_provided_hashes > 0) {
          mpack_write_u8(writer, UCCN_PROVIDED_ARRAY);
          mpack_start_array(writer, num_provided_hashes);
          while (num_provided_hashes > 0) {
            mpack_write_u32(writer, provided_hashes[--num_provided_hashes]);
          }
          mpack_finish_array(writer);
        }
      }
      mpack_finish_map(writer);
    }
    mpack_finish_map(writer);
  }

  return ret;
}

int uccn_process_packet(struct uccn_node_s * node,
                        struct uccn_peer_s * peer,
                        struct buffer_head_s * incoming_packet,
                        struct buffer_head_s * outgoing_packet)
{
  int ret = 0;
  mpack_error_t err;
  mpack_reader_t reader;
  mpack_writer_t writer;
  uint32_t i, group_count;
  uint8_t group_code;

  mpack_reader_init_data(&reader, incoming_packet->data, incoming_packet->length);

  if (mpack_expect_map_max_or_nil(&reader, UCCN_MAX_NUM_GROUPS, &group_count)) {
    for (i = 0; i < group_count && ret == 0; ++i) {
      group_code = mpack_expect_u8(&reader);
      switch (group_code) {
        case UCCN_CONTENT_GROUP:
          ret = uccn_process_content_group(node, peer, &reader);
          if (ret < 0) {
            uccndbg(BACKTRACE_FROM(__LINE__ - 2));
          }
          outgoing_packet->length = 0;
          break;
        case UCCN_LINK_GROUP:
          mpack_writer_init(&writer, outgoing_packet->data, outgoing_packet->size);
          if ((ret = uccn_process_link_group(node, peer, &reader, &writer)) < 0) {
            uccndbg(BACKTRACE_FROM(__LINE__ - 1));
          }
          outgoing_packet->length = mpack_writer_buffer_used(&writer);
          if ((err = mpack_writer_destroy(&writer)) != mpack_ok) {
            uccnerr(RUNTIME_ERR("Failed to build packet: %s", mpack_error_to_string(err)));
            ret = -1;
          }
          break;
        default:
          uccnerr(RUNTIME_ERR("Unknown group code: %hhu", group_code));
          ret = -1;
          break;
      }
    }
    mpack_done_map(&reader);
  }
  mpack_done_array(&reader);
  if ((err = mpack_reader_destroy(&reader)) != mpack_ok) {
    uccnerr(RUNTIME_ERR("Failed to read packet: %s", mpack_error_to_string(err)));
    ret = -1;
  }
  return ret;
}

int uccn_stop(struct uccn_node_s * node) {
  int ret = eventfd_set(&node->stop_event);
  if (ret < 0) {
    uccndbg(BACKTRACE_FROM(__LINE__ - 2));
  }
  return ret;
}

int uccn_node_fini(struct uccn_node_s * node) {
  int ret = 0, iret;

  iret = close(node->socket);
  if (iret < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to close node socket"));
    ret = iret;
  }
  iret = close(node->broadcast_socket);
  if (iret < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to close node socket"));
    ret = iret;
  }
#if CONFIG_UCCN_MULTITHREADED
  iret = pthread_mutex_destroy(&node->mutex);
  if (iret < 0) {
    uccnerr(SYSTEM_ERR_FROM(__LINE__ - 2, "Failed to destroy node mutex"));
    ret = iret;
  }
#endif
  iret = eventfd_fini(&node->stop_event);
  if (iret < 0) {
    uccndbg(BACKTRACE_FROM(__LINE__ - 2));
    ret = iret;
  }
  return ret;
}
