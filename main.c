#include <stdio.h>
#include <uhd.h>


// Parsing the INI File
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>


#include <string>

// Global Vars
boost::property_tree::ptree pt;

// File storage stuff
std::string filepath;

// Usrp Global props
double sampleRate;
double gain;

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
}
