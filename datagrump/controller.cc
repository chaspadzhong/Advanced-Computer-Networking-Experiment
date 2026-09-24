#include <iostream>
#include <cmath>

#include "controller.hh"
#include "timestamp.hh"

using namespace std;

/* Default constructor */
Controller::Controller( const bool debug )
  : debug_( debug ),
    window_size_( 1 ),
    ssthresh_( 16 ),
    congestion_avoidance_ack_count_( 0 ),
    estimated_rtt_( 0 ),
    dev_rtt_( 0 ),
    has_rtt_sample_( false ),
    has_last_ack_( false ),
    last_ack_( 0 )
{}

void Controller::reduce_window()
{
  ssthresh_ = window_size_ > 1 ? window_size_ / 2 : 1;
  window_size_ = 1;
  congestion_avoidance_ack_count_ = 0;
}

/* Get current window size, in datagrams */
unsigned int Controller::window_size()
{
  unsigned int the_window_size = window_size_;

  if ( debug_ ) {
    cerr << "At time " << timestamp_ms()
	 << " window size is " << the_window_size << endl;
  }

  return the_window_size;
}

/* A datagram was sent */
void Controller::datagram_was_sent( const uint64_t sequence_number,
				    /* of the sent datagram */
				    const uint64_t send_timestamp,
                                    /* in milliseconds */
				    const bool after_timeout
				    /* datagram was sent because of a timeout */ )
{
  if ( after_timeout ) {
    reduce_window();
    cout << "time out" << endl;
  }

  if ( debug_ ) {
    cerr << "At time " << send_timestamp
	 << " sent datagram " << sequence_number << " (timeout = " << after_timeout << ")\n";
  }
}

/* An ack was received */
void Controller::ack_received( const uint64_t sequence_number_acked,
			       /* what sequence number was acknowledged */
			       const uint64_t send_timestamp_acked,
			       /* when the acknowledged datagram was sent (sender's clock) */
			       const uint64_t recv_timestamp_acked,
			       /* when the acknowledged datagram was received (receiver's clock)*/
			       const uint64_t timestamp_ack_received )
                               /* when the ack was received (by sender) */
{
  const bool duplicate_ack = has_last_ack_
    and sequence_number_acked == last_ack_;

  if ( duplicate_ack ) {
    cout << "got same ack:" << sequence_number_acked << endl;
    reduce_window();
  } else {
    if ( timestamp_ack_received >= send_timestamp_acked ) {
      const double sample_rtt
        = timestamp_ack_received - send_timestamp_acked;

      if ( not has_rtt_sample_ ) {
        estimated_rtt_ = sample_rtt;
        dev_rtt_ = sample_rtt / 2;
        has_rtt_sample_ = true;
      } else {
        estimated_rtt_ = 0.875 * estimated_rtt_
          + 0.125 * sample_rtt;
        dev_rtt_ = 0.75 * dev_rtt_
          + 0.25 * fabs( sample_rtt - estimated_rtt_ );
      }
    }

    if ( window_size_ < ssthresh_ ) {
      /* Slow start: one new ACK increases cwnd by one datagram. */
      window_size_++;
    } else {
      /* Congestion avoidance: increase by one per window of new ACKs. */
      congestion_avoidance_ack_count_++;
      if ( congestion_avoidance_ack_count_ >= window_size_ ) {
        window_size_++;
        congestion_avoidance_ack_count_ = 0;
      }
    }
  }

  last_ack_ = sequence_number_acked;
  has_last_ack_ = true;

  if ( debug_ ) {
    cerr << "At time " << timestamp_ack_received
	 << " received ack for datagram " << sequence_number_acked
	 << " (send @ time " << send_timestamp_acked
	 << ", received @ time " << recv_timestamp_acked << " by receiver's clock)"
	 << endl;
  }
}

/* How long to wait (in milliseconds) if there are no acks
   before sending one more datagram */
unsigned int Controller::timeout_ms()
{
  if ( not has_rtt_sample_ ) {
    return 1000; /* default until the first RTT sample arrives */
  }

  const double rto = estimated_rtt_ + 4 * dev_rtt_;
  return rto < 1 ? 1 : static_cast<unsigned int>( ceil( rto ) );
}
