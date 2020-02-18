#ifndef ADCSync_H
#define ADCSync_H

#include <vector>
#include <SerialisableObject.h>

class ADCSync : public SerialisableObject{
  friend class boost::serialization::access;

  public:
    int Crate;
    int Card;
    uint64_t Counter;
    uint64_t LastLatch;
    std::vector<uint32_t> Rates;
    ADCSync() {}
    ADCSync(int crate, int card, uint64_t counter, uint64_t lastlatch, std::vector<uint32_t> rates):Crate(crate), Card(card), Counter(counter), LastLatch(lastlatch), Rates(rates) {}
    ~ADCSync(){}

    bool Print(){return false;};

  private:
    template <class Archive> void serialize(Archive& ar, const unsigned int version) {
      ar & Crate;
      ar & Card;
      ar & Counter;
      ar & LastLatch;
      ar & Rates;
    }
};


#endif

