#include "PMTWaveformBuilder.h"
#include <endian.h>
#include "CardData.h"
#include "BoostStore.h"
#include "Store.h"
#include "ADCTrace.h"

struct DecodedFrame{
	int frameid;
	std::vector<uint16_t> samples;
	~DecodedFrame(){
	}
};

std::vector<DecodedFrame> unpackframes(std::vector<uint32_t> bank) {
	uint64_t tempword = 0x0;
	std::vector<DecodedFrame> frames;
	std::vector<uint16_t> samples;
	samples.resize(40);
	for (unsigned int frame = 0; frame<bank.size()/16; ++frame) {
		int sampleindex = 0;
		int wordindex = 16*frame;
		int bitsleft = 0;
		while (sampleindex < 40) {
			if (bitsleft < 12) {
				tempword += ((uint64_t)be32toh(bank[wordindex]))<<bitsleft;
				bitsleft += 32;
				wordindex += 1;
			}
			samples[sampleindex] = tempword&0xfff;
			tempword = tempword>>12;
			bitsleft -= 12;
			sampleindex += 1;
		}
		struct DecodedFrame tempframe;
		tempframe.frameid = be32toh(bank[16*frame+15]);
		tempframe.samples = samples;
		frames.push_back(tempframe);
	}
	return frames;
}

PMTWaveformBuilder::PMTWaveformBuilder():Tool(){}


bool PMTWaveformBuilder::Initialise(std::string configfile, DataModel &data){

  /////////////////// Useful header ///////////////////////
  if(configfile!="") m_variables.Initialise(configfile); // loading config file
  //m_variables.Print();

  m_data= &data; //assigning transient data pointer
  traces = new std::multimap<uint64_t,ADCTrace>(); 
  m_data->CStore.Set("ADCTraces",traces,false); // not persistant
  /////////////////////////////////////////////////////////////////

  return true;
}


bool PMTWaveformBuilder::Execute(){
  if (m_data->CStore.Has("CardDataVector")) {
    std::vector<CardData> cdv;
    m_data->CStore.Get("CardDataVector",cdv);
    for (std::vector<CardData>::iterator cdit = cdv.begin() ; cdit != cdv.end() ; ++cdit) {
      std::vector<DecodedFrame> frames = unpackframes(cdit->Data);
      for (std::vector<DecodedFrame>::iterator dfit = frames.begin() ; dfit != frames.end(); ++dfit) {
        int cid = ((cdit->CardID)*1000)+(dfit->frameid>>24);
        tempwaves[cid].insert(tempwaves[cid].end(),dfit->samples.begin(),dfit->samples.end());
      }
    }
    for (auto chanit=tempwaves.begin(); chanit!=tempwaves.end(); ++chanit) {
      int cid = chanit->first;
      std::deque<uint16_t> &samples = chanit->second;
      cout << "cid: " << cid << endl;
      int last_marker = -1;
      for (unsigned int sit = 0 ; sit != samples.size()-1; ++sit) {
        if (samples[sit] == 0 && samples[sit+1] == 0xFFF) {
          if (last_marker != -1) {
            uint64_t time = 0x0;
            time = (uint64_t)samples[last_marker+2]<<48;
            time += (uint64_t)(0xF&samples[last_marker+3])<<60;
            time += (uint64_t)samples[last_marker+4];
            time += (uint64_t)samples[last_marker+5]<<12;
            time += (uint64_t)samples[last_marker+6]<<24;
            time += (uint64_t)samples[last_marker+7]<<36;
            cout << "time: " << time*8 << " len: " << sit-(last_marker+8) << endl;
            int channel = cid%1000;
            int card = ((cid-channel)/1000)%1000;
            int crate = (cid-card*1000-channel)/1000000;
            std::vector<uint16_t> tempsamples (samples.begin()+last_marker+8,samples.begin()+sit);
            ADCTrace temptrace (crate,card,channel,time,tempsamples);
            traces->emplace(time,temptrace);
            cout << "wf: Traces now holds: " << traces->size() << endl;
            //traces->emplace(time,ADCTrace(crate,card,channel,time,tempsamples));
          }
          last_marker = sit;
        }
      }
      if (last_marker > 0) samples.erase(samples.begin(),samples.begin()+last_marker);
    } // tempwave iterator
  }
  return true;
}


bool PMTWaveformBuilder::Finalise(){

  return true;
}
