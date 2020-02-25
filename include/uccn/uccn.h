#ifndef UCCN_UCCN_H_
#define UCCN_UCCN_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "uccn/config.h"

#include "uccn/common/buffer.h"
#include "uccn/common/time.h"
#include "uccn/utilities/eventfd.h"

struct uccn_resource_s;

typedef ssize_t (*uccn_content_pack_fn)(
    struct uccn_resource_s * resource,
    const void * content,
    struct buffer_head_s ** packet);

typedef ssize_t (*uccn_content_unpack_fn)(
    struct uccn_resource_s * resource,
    const struct buffer_head_s * packet,
    void ** content);

struct uccn_resource_s
{
  const char *path;
  uint32_t hash;

  uccn_content_unpack_fn unpack;
  uccn_content_pack_fn pack;
};

struct uccn_record_typesupport_s;

typedef void *(*uccn_record_allocate_fn)(
    const struct uccn_record_typesupport_s * ts);

typedef ssize_t (*uccn_record_serialize_fn)(
    const struct uccn_record_typesupport_s * ts,
    const void * content,
    struct buffer_head_s * blob);

typedef ssize_t (*uccn_record_deserialize_fn)(
    const struct uccn_record_typesupport_s * ts,
    const struct buffer_head_s * blob,
    void * content);

struct uccn_record_typesupport_s
{
  uccn_record_allocate_fn allocate;
  uccn_record_serialize_fn serialize;
  uccn_record_deserialize_fn deserialize;
};

struct uccn_record_s
{
  struct uccn_resource_s base;
  const struct uccn_record_typesupport_s * ts;
};

struct uccn_peer_s
{
  struct sockaddr_in address;
  char location[INET_ADDRSTRLEN + 7];
  char name[CONFIG_UCCN_MAX_NODENAME_SIZE];

  bool alive;
  struct {
    struct timespec next_remote_deadline;
    struct timespec next_local_deadline;
  } liveliness;

  size_t num_links;
};

struct uccn_node_s;

struct uccn_content_endpoint_s
{
  struct uccn_node_s * node;
  struct uccn_resource_s * resource;
  struct uccn_peer_s * peers[CONFIG_UCCN_MAX_NUM_PEERS];
  size_t num_peers;
};

struct uccn_content_tracker_s;

typedef void (*uccn_content_track_fn)(
    struct uccn_content_tracker_s *tracker,
    void * content);

struct uccn_content_tracker_s
{
  struct uccn_content_endpoint_s endpoint;
  uccn_content_track_fn track;
};

struct uccn_content_provider_s
{
  struct uccn_content_endpoint_s endpoint;
};

struct uccn_network_s
{
  struct in_addr inetaddr;
  struct in_addr netmaskaddr;
};

struct uccn_node_s
{
  int socket;
  struct sockaddr_in address;

  int broadcast_socket;
  struct sockaddr_in broadcast_address;

  char location[INET_ADDRSTRLEN + 7];
  char name[CONFIG_UCCN_MAX_NODENAME_SIZE];

  struct {
    struct buffer_head_s head;
    char default_storage[CONFIG_UCCN_INCOMING_BUFFER_SIZE];
  } incoming_buffer;
  struct {
    struct buffer_head_s head;
    char default_storage[CONFIG_UCCN_OUTGOING_BUFFER_SIZE];
  } outgoing_buffer;
  struct {
    struct buffer_head_s head;
    char default_storage[CONFIG_UCCN_MAX_CONTENT_SIZE];
  } content_buffer;

  struct uccn_peer_s
    peers[CONFIG_UCCN_MAX_NUM_PEERS];
  size_t num_peers;

  struct uccn_content_tracker_s
    trackers[CONFIG_UCCN_MAX_NUM_TRACKERS];
  size_t num_trackers;

  struct uccn_content_provider_s
    providers[CONFIG_UCCN_MAX_NUM_PROVIDERS];
  size_t num_providers;

  struct eventfd_s stop_event;
};

#if defined(__cplusplus)
extern "C"
{
#endif

int uccn_node_init(struct uccn_node_s * node, const char * name,
                   struct uccn_network_s * network);

void uccn_resource_init(struct uccn_resource_s * resource, const char * path);

void uccn_record_init(struct uccn_record_s * record, const char * path,
                      const struct uccn_record_typesupport_s * ts);

struct uccn_content_tracker_s * uccn_track(struct uccn_node_s * node,
                                           struct uccn_resource_s * resource,
                                           uccn_content_track_fn track);

struct uccn_content_provider_s * uccn_advertise(struct uccn_node_s * node,
                                                struct uccn_resource_s * resource);

int uccn_post(struct uccn_content_provider_s * provider, const void * content);

struct uccn_peer_s * uccn_register_peer(struct uccn_node_s * node, struct sockaddr_in * address);

int uccn_spin(struct uccn_node_s * node, struct timespec * timeout);

int uccn_stop(struct uccn_node_s * node);

int uccn_node_fini(struct uccn_node_s * node);

#if defined(__cplusplus)
}
#endif

#endif // UCCN_UCCN_H_
