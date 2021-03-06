/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "engine_gg.hh"

#include <stdexcept>

#include "ggpaths.hh"
#include "optional.hh"
#include "system_runner.hh"
#include "http_response.hh"
#include "remote_response.hh"
#include "units.hh"

using namespace std;
using namespace gg::thunk;

HTTPRequest GGExecutionEngine::generate_request( const Thunk & thunk,
                                                 const string & thunk_hash,
                                                 const bool timelog )
{
  string payload = thunk.execution_payload( thunk_hash, timelog );
  HTTPRequest request;
  request.set_first_line( "POST /cgi-bin/gg/execute.cgi HTTP/1.1" );
  request.add_header( HTTPHeader{ "Content-Length", to_string( payload.size() ) } );
  request.add_header( HTTPHeader{ "Host", "gg-run-server" } );
  request.done_with_headers();

  request.read_in_body( payload );
  assert( request.state() == COMPLETE );

  return request;
}

void GGExecutionEngine::force_thunk( const string & hash,
                                     const Thunk & thunk,
                                     ExecutionLoop & exec_loop )
{
  HTTPRequest request = generate_request( thunk, hash, false );

  TCPSocket socket;
  socket.set_blocking( false );

  try {
    socket.connect( address_ );
    throw runtime_error( "nonblocking connect unexpectedly succeeded immediately" );
  } catch ( const unix_error & e ) {
    if ( e.error_code() == EINPROGRESS ) {
      /* do nothing */
    } else {
      throw;
    }
  }

  exec_loop.add_connection(
    hash,
    [this] ( const string & thunk_hash, const HTTPResponse & http_response )
    {
      running_jobs_--;

      if ( http_response.status_code() != "200" ) {
        throw runtime_error( "HTTP failure: " + http_response.status_code() );
      }

      RemoteResponse response = RemoteResponse::parse_message( http_response.body() );

      if ( response.type != RemoteResponse::Type::SUCCESS ) {
        throw runtime_error( "execution failed." );
      }

      if ( response.thunk_hash != thunk_hash ) {
        cerr << http_response.str() << endl;
        throw runtime_error( "expected output for " + thunk_hash + ", got output for " + response.thunk_hash );
      }

      if (gg::remote::enable_sizelogs())
	    cout << "output_size for thunk " << response.thunk_hash << ": " << response.output_size << "\n";
      
      gg::cache::insert( response.thunk_hash, response.output_hash );
      callback_( response.thunk_hash, response.output_hash, 0 );
    },
    socket, request
  );

  running_jobs_++;
}

size_t GGExecutionEngine::job_count() const
{
  return running_jobs_;
}
