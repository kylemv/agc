#ifndef AGC_IMPL_H
#define AGC_IMPL_H

#include "AGC_base.h"
#include <liquid/liquid.h>

class AGC_i : public AGC_base
{
    ENABLE_LOGGING
    public:
        AGC_i(const char *uuid, const char *label);
        ~AGC_i();
        int serviceFunction();
    private:
        void bandwidthChanged(const float *oldVal, const float *newVal);
        void minPowChanged(const float *oldVal, const float *newVal);
        void maxPowChanged(const float *oldVal, const float *newVal);

        void createAGC_Obj(void);
        void procReal(std::vector<float>& input);
        void procComplex(std::vector<std::complex<float> >& input);

        std::vector<std::complex<float> > cmplx_out;
        std::vector<float> real_out;

        std::vector<float>* output;
        agc_crcf m_complex_agc;
        agc_rrrf m_real_agc;
        unsigned int m_size;
        double m_delta;
        BULKIO::StreamSRI m_sriOut;

        boost::mutex propLock_;
};

#endif // AGC_IMPL_H
