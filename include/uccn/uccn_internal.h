#ifndef UCCN_UCCN_INTERNAL_H_
#define UCCN_UCCN_INTERNAL_H_

#include "uccn/uccn.h"

#include "mpack/mpack.h"

#define UCCN_NODE_NAME          0x8C
#define UCCN_PROVIDED_ARRAY     0x4D
#define UCCN_TRACKED_ARRAY      0xD4
#define UCCN_MAX_NUM_LINK_DATA  3

#define UCCN_LINK_GROUP      0x5A
#define UCCN_CONTENT_GROUP   0xA5
#define UCCN_MAX_NUM_GROUPS  2

#define same_sockaddr_in(a, b)                        \
  (((a)->sin_addr.s_addr == (b)->sin_addr.s_addr) &&  \
   ((a)->sin_port == (b)->sin_port))

#if defined(__cplusplus)
extern "C"
{
#endif

int uccn_prepare_keepalive_packet(struct uccn_node_s * node,
                                  struct buffer_head_s * packet);

int uccn_prepare_discovery_packet(struct uccn_node_s * node,
                                  struct buffer_head_s * packet);

int uccn_assert_liveliness(struct uccn_node_s * node,
                           struct timespec * next_deadline);

int uccn_discover_peers(struct uccn_node_s * node);

int uccn_process_incoming_unicast(struct uccn_node_s * node);

int uccn_process_incoming_broadcast(struct uccn_node_s * node);

int uccn_process_incoming(struct uccn_node_s * node,
                          struct sockaddr_in * origin,
                          struct buffer_head_s * incoming_packet);

int uccn_probe_endpoints(struct uccn_node_s * node,
                         size_t * num_active_trackers,
                         size_t * num_active_providers,
                         struct timespec * next_probe_time);

int uccn_process_content_blob(struct uccn_node_s * node,
                              struct uccn_peer_s * peer,
                              uint32_t hash,
                              struct buffer_head_s * blob);

int uccn_process_content_group(struct uccn_node_s * node,
                               struct uccn_peer_s * peer,
                               mpack_reader_t * reader);

int uccn_link(struct uccn_content_endpoint_s * endpoint,
              struct uccn_peer_s * peer);

int uccn_unlink(struct uccn_content_endpoint_s * endpoint,
                struct uccn_peer_s * peer);

int uccn_link_trackers(struct uccn_node_s * node,
                       struct uccn_peer_s * peer,
                       uint32_t hash);

int uccn_link_providers(struct uccn_node_s * node,
                        struct uccn_peer_s * peer,
                        uint32_t hash);

int uccn_unlink_providers(struct uccn_node_s * node,
                          struct uccn_peer_s * peer);

int uccn_unlink_trackers(struct uccn_node_s * node,
                         struct uccn_peer_s * peer);

int uccn_process_link_group(struct uccn_node_s * node,
                            struct uccn_peer_s * peer,
                            mpack_reader_t * reader,
                            mpack_writer_t * writer);

int uccn_process_packet(struct uccn_node_s * node,
                        struct uccn_peer_s * peer,
                        struct buffer_head_s * incoming_packet,
                        struct buffer_head_s * outgoing_packet);

#if defined(__cplusplus)
}
#endif

#endif  // UCCN_UCCN_INTERNAL_H_
