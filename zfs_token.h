
#ifndef ZFS_TOKEN_H
#define ZFS_TOKEN_H

char *forge_receive_resume_token(
    uint64_t fromguid,
    uint64_t object,
    uint64_t offset,
    uint64_t bytes,
    uint64_t toguid,
    char *toname,
    boolean_t largeblockok,
    boolean_t embedok,
    boolean_t compressok,
    boolean_t rawok);

#endif