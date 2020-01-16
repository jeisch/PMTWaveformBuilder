#ifndef ADCTrace_H
#define ADCTrace_H

#include <vector>
#include <SerialisableObject.h>

class ADCTrace : public SerialisableObject{
  friend class boost::serialization::access;

  public:
    int Crate;
    int Card;
    int Channel;
    uint64_t Start;
    std::vector<uint16_t> Samples;
    ADCTrace() {}
    ADCTrace(int crate, int card, int channel, uint64_t start, std::vector<uint16_t> samples):Crate(crate), Card(card), Channel(channel), Start(start), Samples(samples) {}
    ~ADCTrace(){}

    bool Print(){return false;};

  private:
    template <class Archive> void serialize(Archive& ar, const unsigned int version) {
      ar & Crate;
      ar & Card;
      ar & Channel;
      ar & Start;
      ar & Samples;
    }
};


#endif
