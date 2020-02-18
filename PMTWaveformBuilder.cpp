#include "PMTWaveformBuilder.h"
#include <endian.h>
#include "CardData.h"
#include "BoostStore.h"
#include "Store.h"
#include "ADCTrace.h"
#include "ADCSync.h"

struct WaveformFrame{
	int frameid;
	std::vector<uint16_t> samples;
	~WaveformFrame(){
	}
};

struct SyncFrame{
	int frameid;
	uint64_t Counter;
	uint64_t LastLatch;
	std::vector<uint32_t> Rates;
	~SyncFrame(){
	}
};

bool unpackframes(std::vector<uint32_t> bank, std::vector<WaveformFrame> &waveframes, std::vector<SyncFrame> &syncframes) {
	uint64_t tempword = 0x0;
	for (unsigned int frame = 0; frame<bank.size()/16; ++frame) {
		uint32_t frameid = be32toh(bank[16*frame+15]);
		if ((frameid>>24)==10) {
			SyncFrame tempsync;
			tempsync.Counter = be32toh(bank[16*frame+1])<<32+be32toh(bank[16*frame]);
			tempsync.LastLatch = be32toh(bank[16*frame+3])<<32+be32toh(bank[16*frame+2]);
			tempsync.Rates.resize(10);
			for (int i = 0; i!=10; ++i) {
				tempsync.Rates[i] = be32toh(bank[16*frame+4+i]);
			}
			syncframes.push_back(tempsync);
		}
		else if ((frameid>>24)<10) {
			std::vector<uint16_t> samples;
			samples.resize(40);
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
			struct WaveformFrame tempframe;
			tempframe.frameid = be32toh(bank[16*frame+15]);
			tempframe.samples = samples;
			waveframes.push_back(tempframe);
		}
		else {
			cout << "FrameID not known" << std::endl;
			return false;
		}
	} // loop over frames
	return true;
}


PMTWaveformBuilder::PMTWaveformBuilder():Tool(){}


bool PMTWaveformBuilder::Initialise(std::string configfile, DataModel &data){

  /////////////////// Useful header ///////////////////////
  if(configfile!="") m_variables.Initialise(configfile); // loading config file
  //m_variables.Print();

  m_data= &data; //assigning transient data pointer
  traces = new std::multimap<uint64_t,ADCTrace>(); 
  m_data->CStore.Set("ADCTraces",traces,false); // not persistant
  syncs = new std::vector<ADCSync>();
  m_data->CStore.Set("ADCSyncs",syncs,false); // not persistant
  /////////////////////////////////////////////////////////////////

  fifoerrors = 0;
  return true;
}


bool PMTWaveformBuilder::Execute(){
  if (m_data->CStore.Has("CardDataVector")) {
    std::vector<CardData> cdv;
    m_data->CStore.Get("CardDataVector",cdv);
    for (auto cdit = cdv.begin() ; cdit != cdv.end() ; ++cdit) {
      std::vector<WaveformFrame> waveframes;
      std::vector<SyncFrame> syncframes;
      unpackframes(cdit->Data,waveframes,syncframes);
      for (auto dfit = waveframes.begin() ; dfit != waveframes.end(); ++dfit) {
        int cid = ((cdit->CardID)*1000)+(dfit->frameid>>24);
        tempwaves[cid].insert(tempwaves[cid].end(),dfit->samples.begin(),dfit->samples.end());
      }
      if (cdit->FIFOstate>0) {
        fifoerrors += 1;
	for (int ch = 1; ch<=4; ++ch) {
          tempwaves[(cdit->CardID*1000)+ch].push_back(0xF000+cdit->FIFOstate);
        }
      }
      for (auto sit = syncframes.begin(); sit != syncframes.end(); ++sit) {
        ADCSync tempsync(cdit->CardID/1000,cdit->CardID%1000,sit->Counter,sit->LastLatch,sit->Rates);
        syncs->push_back(tempsync);
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
            if (sit-last_marker < 8) continue;  // hack for marker in timestamp
            uint64_t time = 0x0;
            time = (uint64_t)samples[last_marker+2]<<48;
            time += (uint64_t)(0xF&samples[last_marker+3])<<60;
            time += (uint64_t)samples[last_marker+4];
            time += (uint64_t)samples[last_marker+5]<<12;
            time += (uint64_t)samples[last_marker+6]<<24;
            time += (uint64_t)samples[last_marker+7]<<36;
            cout << "cid: " << cid << " time: " << time*8 << " len: " << sit-(last_marker+8) << endl;
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
  cout << "Fifoerrors :" << fifoerrors << std::endl;
  return true;
}
