// basic file operations
#include <iostream>
#include <fstream>

using namespace std;

int main(int argc, char* argv[]) 
{
  if (argc < 5) {
    cerr << "Usage " << argv[0] << " sample_format sample_rate channels duration" << endl;
    exit(1);
  }

  int len(0);
  string format(argv[1]);
  if (format == "S16_LE")
    len = 2;
  else if (format == "S24_3LE") 
    len = 3;
  else if (format == "S32_LE") 
    len = 4;
  else {
    cerr << "Unsupported format " <<  format << endl;
    exit(1);
  }

  int rate(atoi(argv[2]));
  if (rate != 44100 && rate != 48000 && rate != 96000) {
    cerr << "Unsupported rate " << rate << endl;
    exit(1);
  }

  int channels = atoi(argv[3]);
  if (channels != 1 && channels != 2 && channels != 4 && channels != 8) {
    cerr << "Unsupported channels " << channels << endl;
    exit(1);
  }

  int duration = atoi(argv[4]);
  if (duration > 10 || duration < 1) {
    cerr << "Unsupported duration " << duration << " minutes" << endl;
    exit(1);
  }

  int secs(duration * 60);
  unsigned char byte(0);
  fstream myfile;
  myfile.open("test.raw", ios::out|ios::binary);
  while(secs--) {
    int samples(rate);
    while (samples--) {
      int ch(channels);
      while (ch--) {
        myfile.put(byte);
	if (len > 1) {
          myfile.put(byte+1);
	  if (len > 2) {
            myfile.put(byte+2);
	    if (len > 3)
              myfile.put(byte+3);
          }
	}
      }
      byte+=len;
    }
  }
  myfile.close();
  return 0;

}
