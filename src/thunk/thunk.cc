/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "thunk.hh"

#include <unistd.h>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <cajun/json/reader.h>
#include <cajun/json/writer.h>
#include <cajun/json/elements.h>

#include "system_runner.hh"
#include "thunk_writer.hh"
#include "temp_file.hh"
#include "placeholder.hh"
#include "digest.hh"
#include "ggpaths.hh"

using namespace std;
using namespace gg;
using namespace gg::thunk;
using namespace CryptoPP;

Thunk::Thunk( const string & outfile, const Function & function,
              const vector<InFile> & infiles )
  : outfile_( roost::path( outfile ).lexically_normal().string() ),
    function_( function ), infiles_( infiles ), order_( compute_order() )
{}

Thunk::Thunk( const gg::protobuf::Thunk & thunk_proto )
  : outfile_( thunk_proto.outfile() ), function_( thunk_proto.function() ),
    infiles_(), order_()
{
  for ( protobuf::InFile infile : thunk_proto.infiles() ) {
    infiles_.push_back( { infile } );
  }

  order_ = compute_order();
}

int Thunk::execute( const string & thunk_hash ) const
{
  if ( order_ != 1 ) {
    throw runtime_error( "cannot execute thunk with order != 1" );
  }

  bool verbose = ( getenv( "GG_VERBOSE" ) != nullptr );

  // preparing argv
  vector<string> args = function_.args();

  /* do we need to replace a filename with its hash? */
  for ( string & arg : args ) {
    const size_t replace_begin = arg.find( BEGIN_REPLACE );
    if ( replace_begin == string::npos ) {
      continue;
    }

    const size_t filename_begin = replace_begin + BEGIN_REPLACE.length();

    const size_t replace_end = arg.find( END_REPLACE, filename_begin );
    if ( replace_end == string::npos ) {
      throw runtime_error( "invalid GG argument replacement: " + arg );
    }

    const string filename = arg.substr( filename_begin,
                                        replace_end - filename_begin );

    const string replacement = filename_to_hash( filename );

    arg.replace( replace_begin,
                 BEGIN_REPLACE.length() + filename.length() + END_REPLACE.length(),
                 replacement );
  }

  args.insert( args.begin(), function_.exe() );

  const roost::path thunk_path = gg::paths::blob_path( thunk_hash );

  // preparing envp
  const vector<string> & f_envars = function_.envars();
  vector<string> envars = {
    "__GG_THUNK_PATH__=" + thunk_path.string(),
    "__GG_DIR__=" + gg::paths::blobs().string(),
    "__GG_ENABLED__=1",
  };

  if ( verbose ) {
    envars.emplace_back( "__GG_VERBOSE__=1" );
  }

  envars.insert( envars.end(), f_envars.begin(), f_envars.end() );

  int retval;

  if ( verbose ) {
    string exec_string = "+ exec(" + thunk_hash + ") {"
                       + roost::rbasename( function_.exe() ).string()
                       + "}\n";

    cerr << exec_string;
  }

  if ( ( retval = ezexec( gg::paths::blob_path( function_.hash() ).string(), args, envars ) ) < 0 ) {
    throw runtime_error( "execvpe failed" );
  }

  return retval;
}

std::string Thunk::execution_payload( const std::string & thunk_hash,
                                      const bool timelog ) const
{
  string base64_thunk;

  StringSource s( ThunkWriter::serialize_thunk( *this ), true,
                new Base64Encoder( new StringSink( base64_thunk ), false ) );

  json::Object lambda_event;
  lambda_event[ "thunk_hash" ] = json::String( thunk_hash );
  lambda_event[ "s3_bucket" ] = json::String( gg::remote::s3_bucket() );
  lambda_event[ "s3_region" ] = json::String( gg::remote::s3_region() );
  lambda_event[ "thunk_data" ] = json::String( base64_thunk );
  lambda_event[ "hostaddr" ] = json::String( gg::remote::redis_hostaddr() );
  lambda_event[ "namenode_addr" ] = json::String( gg::remote::crail_namenode_addr() );
  lambda_event[ "private_hostaddr" ] = json::String( gg::remote::redis_private_hostaddr() );
  lambda_event[ "redis_enabled" ] = json::Boolean( gg::remote::redis_enabled() );
  lambda_event[ "crail_enabled" ] = json::Boolean( gg::remote::crail_enabled() );

  if ( timelog ) {
    lambda_event[ "timelog" ] = json::Boolean( true );
  }

  json::Array lambda_event_infiles;

  for ( const InFile & infile : infiles_ ) {
    if ( infile.type() == InFile::Type::DUMMY_DIRECTORY ) {
      continue;
    }

    json::Object event_infile;
    event_infile[ "hash" ] = json::String( infile.content_hash() );
    event_infile[ "size" ] = json::Number( infile.size() );
    event_infile[ "executable" ] = json::Boolean(  infile.type() == InFile::Type::EXECUTABLE );

    lambda_event_infiles.Insert( event_infile );
  }

  lambda_event[ "infiles" ] = lambda_event_infiles;

  ostringstream oss;
  json::Writer::Write( lambda_event, oss );
  return oss.str();
}

size_t Thunk::compute_order() const
{
  size_t order = 0;

  for ( const InFile & infile : infiles_ ) {
    order = max( infile.order(), order );
  }

  return order + 1;
}

protobuf::Thunk Thunk::to_protobuf() const
{
  protobuf::Thunk thunk;

  thunk.set_outfile( outfile_ );
  *thunk.mutable_function() = function_.to_protobuf();

  for ( const InFile & infile : infiles_ ) {
    *thunk.add_infiles() = infile.to_protobuf();
  }

  return thunk;
}

void put_file( const roost::path & src, const roost::path & dst )
{
  if ( roost::exists( dst ) ) {
    /* XXX we might want to implement strict checks, like hash check */
    return;
  }

  roost::copy_then_rename( src, dst );
}

void Thunk::collect_infiles() const
{
  for ( InFile infile : infiles_ ) {
    if ( infile.content_hash().length() == 0 ) {
      /* this is a directory, not a file. no need to copy anything */
      continue;
    }

    roost::path source_path = infile.real_filename();
    roost::path target_path = gg::paths::blob_path( infile.content_hash() );
    put_file( source_path, target_path );
  }
}

string Thunk::store( const bool create_placeholder ) const
{
  collect_infiles();

  const string thunk_hash = ThunkWriter::write_thunk( *this );

  if ( create_placeholder ) {
    ThunkPlaceholder placeholder { thunk_hash, order(),
                                   roost::file_size( paths::blob_path( thunk_hash ) ) };
    placeholder.write( outfile() );
  }

  return thunk_hash;
}

bool Thunk::operator==( const Thunk & other ) const
{
  return ( outfile_ == other.outfile_ ) and
         ( function_ == other.function_ ) and
         ( infiles_ == other.infiles_ ) and
         ( order_ == other.order_ );
}

string Thunk::executable_hash() const
{
  vector<string> executable_hashes;

  for ( const InFile & infile : infiles_ ) {
    if ( infile.type() == InFile::Type::EXECUTABLE ) {
      executable_hashes.push_back( infile.content_hash() );
    }
  }

  sort( executable_hashes.begin(), executable_hashes.end() );

  const string combined_hashes = accumulate( executable_hashes.begin(),
                                             executable_hashes.end(),
                                             string {} );

  return digest::sha256( combined_hashes, true );
}

void Thunk::update_infile( const string & old_hash, const string & new_hash,
                           const size_t new_order, const off_t new_size )
{
  bool found = false;

  for ( size_t i = 0; i < infiles_.size(); i++ ) {
    if ( infiles_[ i ].content_hash() == old_hash ) {
      InFile new_infile { infiles_[ i ].filename(), infiles_[ i ].real_filename(),
                          new_hash, new_order, new_size, infiles_[ i ].type() };

      infiles_[ i ] = new_infile;
      order_ = compute_order();
      found = true;
    }
  }

  if ( not found ) {
    throw runtime_error( "infile doesn't exist: " + old_hash );
  }
}

string Thunk::filename_to_hash( const string & filename ) const
{
  const string normalized_filename = roost::path( filename ).lexically_normal().string();
  for ( const InFile & infile : infiles_ ) {
    if ( infile.filename() == normalized_filename ) {
      return infile.content_hash();
    }
  }

  throw runtime_error( "filename " + filename + " not found in thunk" );
}

unordered_map<string, Permissions>
Thunk::get_allowed_files( const std::string & thunk_hash ) const
{
  unordered_map<string, Permissions> allowed_files;

  for ( const InFile & infile : infiles() ) {
    if ( infile.content_hash().length() ) {
      if ( infile.type() == InFile::Type::FILE ) {
        allowed_files[ gg::paths::blob_path( infile.content_hash() ).string() ] = { true, false, false };
      }
      else if ( infile.type() == InFile::Type::EXECUTABLE ) {
        allowed_files[ gg::paths::blob_path( infile.content_hash() ).string() ] = { true, false, true };
      }
    }
    else {
      allowed_files[ infile.filename() ] = { true, false, false };
    }
  }

  allowed_files[ gg::paths::blobs().string() ] = { true, false, false };
  allowed_files[ gg::paths::blob_path( thunk_hash ).string() ] = { true, false, false };
  allowed_files[ outfile() ] = { true, true, false };

  return allowed_files;
}

size_t Thunk::infiles_size( const bool include_executables ) const
{
  size_t total_size = 0;

  for ( const InFile & infile : infiles_ ) {
    if ( infile.type() == InFile::Type::FILE or
         ( include_executables and infile.type() == InFile::Type::EXECUTABLE ) ) {
      total_size += infile.size();
    }
  }

  return total_size;
}
