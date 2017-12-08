/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef STORAGE_BACKEND_REDIS_HH
#define STORAGE_BACKEND_REDIS_HH

#include "backend.hh"
#include "aws.hh"
#include "redis.hh"

class RedisStorageBackend : public StorageBackend
{
private:
  RedisClient client_;

public:
  RedisStorageBackend( const AWSCredentials & credentials,
                       const std::string & hostname,
                       const int port );

  void put( const std::vector<storage::PutRequest> & requests,
            const PutCallback & success_callback = []( const storage::PutRequest & ){} ) override;

  void get( const std::vector<storage::GetRequest> & requests,
            const GetCallback & success_callback = []( const storage::GetRequest & ){} ) override;

};

#endif /* STORAGE_BACKEND_REDIS_HH */
