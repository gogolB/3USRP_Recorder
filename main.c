#include <stdio.h>

// UHD stuff
#include <uhd.h>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/types/device_addr.hpp>
#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/subdev_spec.hpp>


// Parsing the INI File
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>

#include <string>
#include <complex>
#include <csignal>

// Moodycamel SPSC lock-less queue
#include "readerwriterqueue.h"
#include "atomicops.h"


// Global Vars
boost::property_tree::ptree pt;

// File storage stuff
std::string filepath;

// Usrp Global props
double sampleRate;
double gain;
int total_num_samps;

// USRP 0 Params
std::string devaddr0;
std::string filename0;
double centerfrq0;
// USRP 1 Params
std::string devaddr1;
std::string filename1;
double centerfrq1;
// USRP 2 Params
std::string devaddr2;
std::string filename2;
double centerfrq2;


// Multi USRP sptr;
uhd::usrp::multi_usrp::sptr dev;

static bool stop_signal_called = false;
void sig_int_handler(int){stop_signal_called = true;}

void createUSRPs(void)
{
	// Preping to create the multi USRP
	uhd::device_addr_t dev_addr;
	
	dev_addr["addr0"] = devaddr0;
	dev_addr["addr1"] = devaddr1;
	dev_addr["addr2"] = devaddr2;
	
	// Create the Multi USRP
	dev = uhd::usrp::multi_usrp::make(dev_addr);

	// Set all the subdevs

	uhd::usrp::subdev_spec_t subdev("A:A");

	dev->set_rx_subdev_spec(subdev,0);
	dev->set_rx_subdev_spec(subdev,1);
	dev->set_rx_subdev_spec(subdev,2);

	// Make sure the clock source is external
	dev->set_clock_source("external");

	// Set the sample rate please note that I am scaling up the double
	// this means that the samplerate in the config file should be in Msps.
	dev->set_rx_rate(sampleRate*1e6);

	// Set the center freq
	uhd::tune_request_t tune_request0(centerfrq0*1e6);
	dev->set_rx_freq(tune_request0,0);
	
	uhd::tune_request_t tune_request1(centerfrq1*1e6);
	dev->set_rx_freq(tune_request1,1);
	
	uhd::tune_request_t tune_request2(centerfrq2*1e6);
	dev->set_rx_freq(tune_request2,2);

	// Set the gains
	dev->set_rx_gain(gain,uhd::usrp::multi_usrp::ALL_CHANS);
}

void write_to_file_thread(const std::string file, moodycamel::BlockingReaderWriterQueue<std::complex<float> > &q) {
	std::ofstream outfile;
	outfile.open(file.c_str(), std::ofstream::binary);
	std::complex<float> data;
	if (outfile.is_open()) {
		q.wait_dequeue(data);
		outfile.write((const char*)&data, sizeof(data));
		while (q.try_dequeue(data)) {
			outfile.write((const char*)&data, sizeof(data));
		}
		printf("File %s written!\n", file.c_str());
	}
	else {
		printf("File %s failed to open.\n", file.c_str());
	}
}

int main(int argc, char ** argv)
{
	if(argc < 2)
	{
		// Automatically terminate if no config file provided.
		printf("Not enough parameters...\n\t Ex: ./datarec cfg_file.ini\n");
		return -1;
	}

	// Open the provided file.
	printf("Attemping to open file: %s\n", argv[1]);
	boost::property_tree::ini_parser::read_ini("config.ini", pt);


	// Get the global params.
	filepath = pt.get<std::string>("Global.filepath");
	sampleRate = std::stod(pt.get<std::string>("Global.samplerate"));
	gain = std::stod(pt.get<std::string>("Global.gain"));
	total_num_samps = std::stoi(pt.get<std::string>("Global.total_num_samps"));
	sync = pt.get<std::string>("Global.sync"); // "now", "pps", or "mimo"

	printf("All files will be stored in directory %s.\nSample Rate is %0.2f MSps\nUSRP Gain is %fdB\n",filepath.c_str(), sampleRate, gain);
	

	// Get USRP 0 params
	devaddr0 = pt.get<std::string>("USRP0.deviceaddr");
	filename0 = pt.get<std::string>("USRP0.filename");
	centerfrq0 = std::stod(pt.get<std::string>("USRP0.centerfrq"));

	printf("Using USRP [%s] to record to %s%s with a center frq of %0.2f Mhz\n",devaddr0.c_str(), filepath.c_str(),filename0.c_str(), centerfrq0);

	// Get USRP 1 params
	devaddr1 = pt.get<std::string>("USRP1.deviceaddr");
	filename1 = pt.get<std::string>("USRP1.filename");
	centerfrq1 = std::stod(pt.get<std::string>("USRP1.centerfrq"));

	printf("Using USRP [%s] to record to %s%s with a center frq of %0.2f Mhz\n",devaddr1.c_str(), filepath.c_str(),filename1.c_str(), centerfrq1);


	// Get USRP 0 params
	devaddr2 = pt.get<std::string>("USRP2.deviceaddr");
	filename2 = pt.get<std::string>("USRP2.filename");
	centerfrq2 = std::stod(pt.get<std::string>("USRP2.centerfrq"));

	printf("Using USRP [%s] to record to %s%s with a center frq of %0.2f Mhz\n",devaddr2.c_str(), filepath.c_str(),filename2.c_str(), centerfrq2);


	// Setup rx streamer
	// For external clock source, set time
	dev->set_time_unknown_pps(uhd::time_spec_t(0.0));
	boost::this_thread::sleep(boost::chrono::seconds(1)); // Wait for pps sync pulse

	// Create a receive streamer
	uhd::stream_args_t stream_args("fc32"); //complex floats
	stream_args.channels = 0;
	uhd::rx_streamer::sptr rx_stream = dev->get_rx_stream(stream_args);

	// Setup streaming
	printf("\nBegin streaming %d samples...\n", total_num_samps);
	uhd::stream_cmd_t stream_cmd((total_num_samps == 0) ?
		uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS :
		uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE
   );
   stream_cmd.num_samps = total_num_samps;
	stream_cmd.stream_now = true;
	stream_cmd.time_spec = uhd::time_spec_t();
	rx_stream->issue_stream_cmd(stream_cmd); 
	if (total_num_samps == 0) {
		std::signal(SIGINT, &sig_int_handler);
		printf("Press Ctrl + C to stop streaming...\n");
	}

	// Allocate buffers to receive with samples (one buffer per channel)
	const size_t samps_per_buff = rx_stream->get_max_num_samps();
	std::vector<std::vector<std::complex<float> > > buffs(
		dev->get_rx_num_channels(), std::vector<std::complex<float> >(samps_per_buff)
	);

	// Create a vector of pointers to point to each of the channel buffers
	std::vector<std::complex<float> *> buff_ptrs;
	for (size_t i = 0; i < buffs.size(); i++) {
		buff_ptrs.push_back(&buffs[i].front());
	}

	moodycamel::BlockingReaderWriterQueue<std::complex<float> > q0;
	moodycamel::BlockingReaderWriterQueue<std::complex<float> > q1;
	moodycamel::BlockingReaderWriterQueue<std::complex<float> > q2;

	// Consumer threads
	boost::thread c0(write_to_file_thread, filepath + filename0, q0);
	boost::thread c1(write_to_file_thread, filepath + filename1, q1);
	boost::thread c2(write_to_file_thread, filepath + filename2, q2);

	// Streaming loop
	size_t num_acc_samps = 0; //number of accumulated samples
	bool overflow_message = true;

	while (!stop_signal_called && (num_acc_samps < total_num_samps)) {
		size_t num_rx_samps = rx_stream->recv(
			buff_ptrs, samps_per_buff, md
		);

		// Handle the error code
		if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
			printf("Timeout while streaming\n");
			break;
		}
		if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW) {
			if (overflow_message) {
				overflow_message = false;
				std::cerr << boost::format(
					"Got an overflow indication. Please consider the following:\n"
					"  Your write medium must sustain a rate of %fMB/s.\n"
					"  Dropped samples will not be written to the file.\n"
					"  This message will not appear again.\n"
				) % (dev->get_rx_rate()*sizeof(std::complex<float>)/1e6);
			}
			continue;
		}
		if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
			printf("Receiver error: %s\n", md.strerror().c_str());
      }
		num_acc_samps += num_rx_samps;
	}

	if (num_acc_samps < total_num_samps && !stop_signal_called) {
		printf("Received timeout before all samples received...\n");
	}

	c1.join();
	c2.join();
	c3.join();

	// Finished
	printf("\nDone!\n\n");

	return EXIT_SUCCESS;
}
