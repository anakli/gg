/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef REDIS_HH
#define REDIS_HH

#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <functional>

#include "aws.hh"
#include "http_request.hh"
#include "path.hh"
#include "optional.hh"
#include "storage_requests.hh"

#include <iostream>
#include <redox.hpp>

struct RedisClientConfig
{
  std::string hostaddr {};
  int port = 6379;
  //size_t max_threads { 32 };
  //size_t max_batch_size { 32 };
};

class RedisClient
{
private:
  AWSCredentials credentials_;
  RedisClientConfig config_;

public:
  RedisClient( const AWSCredentials & credentials,
			   const RedisClientConfig & config = {} );

  void download_file( const std::string & object,
                      const roost::path & filename );

  void upload_files( const std::vector<storage::PutRequest> & upload_requests,
                     const std::function<void( const storage::PutRequest & )> & success_callback
                       = []( const storage::PutRequest & ){} );

  void download_files( const std::vector<storage::GetRequest> & download_requests,
                       const std::function<void( const storage::GetRequest & )> & success_callback
                         = []( const storage::GetRequest & ){} );
};

#endif /* REDIS_HH */
