/* simple UDP receiver that acknowledges every datagram */

#include <cstdlib>
#include <iostream>

#include "socket.hh"
#include "contest_message.hh"

using namespace std;

int main( int argc, char *argv[] )
{
   /* check the command-line arguments */
  if ( argc < 1 ) { /* for sticklers */
    abort();
  }

  if ( argc != 2 ) {
    cerr << "Usage: " << argv[ 0 ] << " PORT" << endl;
    return EXIT_FAILURE;
  }

  /* create UDP socket for incoming datagrams */
  UDPSocket socket;

  /* turn on timestamps on receipt */
  socket.set_timestamps();

  /* "bind" the socket to the user-specified local port number */
  socket.bind( Address( "::0", argv[ 1 ] ) );

  cerr << "Listening on " << socket.local_address().to_string() << endl;

  uint64_t sequence_number = 0;
  uint64_t should_ack_num = 0;

  /* Loop and acknowledge every incoming datagram back to its source */
  while ( true ) {
    const UDPSocket::received_datagram recd = socket.recv();
    ContestMessage message = recd.payload;

    if ( message.header.sequence_number == should_ack_num ) {
      should_ack_num++;
    } else {
      /* Acknowledge the last contiguous packet to create a duplicate ACK. */
      message.header.sequence_number = should_ack_num == 0
        ? 0 : should_ack_num - 1;
    }

    // cerr << "Received message: message_type="
    //      << ( message.is_ack() ? "ACK" : "DATA" )
    //      << ", sequence_number=" << message.header.sequence_number
    //      << ", send_timestamp=" << message.header.send_timestamp
    //      << ", payload_length=" << message.payload.length()
    //      << endl;

    /* assemble the acknowledgment */
    message.transform_into_ack( sequence_number++, recd.timestamp );

    /* timestamp the ack just before sending */
    message.set_send_timestamp();

    /* send the ack */
    socket.sendto( recd.source_address, message.to_string() );

  }

  return EXIT_SUCCESS;
}
