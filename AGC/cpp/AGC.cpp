/**************************************************************************

    This is the component code. This file contains the child class where
    custom functionality can be added to the component. Custom
    functionality to the base class can be extended here. Access to
    the ports can also be done from this class

**************************************************************************/

#include "AGC.h"

PREPARE_LOGGING(AGC_i)

AGC_i::AGC_i(const char *uuid, const char *label) :
    AGC_base(uuid, label)
{
	addPropertyChangeListener("bandwidth", this, &AGC_i::bandwidthChanged);
	addPropertyChangeListener("minPower", this, &AGC_i::minPowChanged);
	addPropertyChangeListener("maxPower", this, &AGC_i::maxPowChanged);

	m_complex_agc = NULL;
	m_real_agc = NULL;
	m_delta = 0.0;
	m_size = 0;
	m_sriOut = bulkio::sri::create("AGC_OUT");
	output = NULL;
}

AGC_i::~AGC_i()
{
	if(m_complex_agc){
		agc_crcf_destroy(m_complex_agc);
		m_complex_agc = NULL;
	}
	if(m_real_agc){
		agc_rrrf_destroy(m_real_agc);
		m_real_agc = NULL;
	}
}
/***********************Property Change Listeners****************************/
void AGC_i::bandwidthChanged(const float *oldVal, const float *newVal)
{
	boost::mutex::scoped_lock lock(propLock_);

	if(*newVal < 0){
		std::cerr << "Warning! - Bandwidth must be positive!"<<std::endl;
		std::cerr << "Keeping Bandwidth at "<<*oldVal<<std::endl;
		bandwidth = *oldVal;
	}
	else{
		bandwidth = *newVal;
	}
	createAGC_Obj();

}
void AGC_i::minPowChanged(const float *oldVal, const float *newVal)
{
	boost::mutex::scoped_lock lock(propLock_);
	minPower = *newVal;
	createAGC_Obj();
}
void AGC_i::maxPowChanged(const float *oldVal, const float *newVal)
{
	boost::mutex::scoped_lock lock(propLock_);
	maxPower = *newVal;
	createAGC_Obj();
}

/*********************Processing Functions************************************/
void AGC_i::procReal(std::vector<float>& input)
{
	//std::cout<<"beginning processing for real input"<<std::endl;
	agc_rrrf_reset(m_real_agc);
	//std::cout<<"reset agc"<<std::endl;
	m_size = input.size();
	real_out.resize(m_size);
	//std::cout<<"got size"<<std::endl;
	for(unsigned int i = 0; i < m_size; i++)
		agc_rrrf_execute(m_real_agc, input[i], &real_out[i]);
	//std::cout<<"processed input"<<std::endl;
	output = &real_out;
	//std::cout<<"assigned output"<<std::endl;
}

void AGC_i::procComplex(std::vector<std::complex<float> >& input)
{
	agc_crcf_reset(m_complex_agc);
	m_size = input.size();
	cmplx_out.resize(m_size);
	for(unsigned int i = 0; i < m_size; i++)
		agc_crcf_execute(m_complex_agc, input[i], &cmplx_out[i]);
	output = (std::vector<float>*)&cmplx_out;
}
/***********************************************************************************************

    Basic functionality:

        The service function is called by the serviceThread object (of type ProcessThread).
        This call happens immediately after the previous call if the return value for
        the previous call was NORMAL.
        If the return value for the previous call was NOOP, then the serviceThread waits
        an amount of time defined in the serviceThread's constructor.
        
    SRI:
        To create a StreamSRI object, use the following code:
                std::string stream_id = "testStream";
                BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);

	Time:
	    To create a PrecisionUTCTime object, use the following code:
                BULKIO::PrecisionUTCTime tstamp = bulkio::time::utils::now();

        
    Ports:

        Data is passed to the serviceFunction through the getPacket call (BULKIO only).
        The dataTransfer class is a port-specific class, so each port implementing the
        BULKIO interface will have its own type-specific dataTransfer.

        The argument to the getPacket function is a floating point number that specifies
        the time to wait in seconds. A zero value is non-blocking. A negative value
        is blocking.  Constants have been defined for these values, bulkio::Const::BLOCKING and
        bulkio::Const::NON_BLOCKING.

        Each received dataTransfer is owned by serviceFunction and *MUST* be
        explicitly deallocated.

        To send data using a BULKIO interface, a convenience interface has been added 
        that takes a std::vector as the data input

        NOTE: If you have a BULKIO dataSDDS or dataVITA49  port, you must manually call 
              "port->updateStats()" to update the port statistics when appropriate.

        Example:
            // this example assumes that the component has two ports:
            //  A provides (input) port of type bulkio::InShortPort called short_in
            //  A uses (output) port of type bulkio::OutFloatPort called float_out
            // The mapping between the port and the class is found
            // in the component base class header file

            bulkio::InShortPort::dataTransfer *tmp = short_in->getPacket(bulkio::Const::BLOCKING);
            if (not tmp) { // No data is available
                return NOOP;
            }

            std::vector<float> outputData;
            outputData.resize(tmp->dataBuffer.size());
            for (unsigned int i=0; i<tmp->dataBuffer.size(); i++) {
                outputData[i] = (float)tmp->dataBuffer[i];
            }

            // NOTE: You must make at least one valid pushSRI call
            if (tmp->sriChanged) {
                float_out->pushSRI(tmp->SRI);
            }
            float_out->pushPacket(outputData, tmp->T, tmp->EOS, tmp->streamID);

            delete tmp; // IMPORTANT: MUST RELEASE THE RECEIVED DATA BLOCK
            return NORMAL;

        If working with complex data (i.e., the "mode" on the SRI is set to
        true), the std::vector passed from/to BulkIO can be typecast to/from
        std::vector< std::complex<dataType> >.  For example, for short data:

            bulkio::InShortPort::dataTransfer *tmp = myInput->getPacket(bulkio::Const::BLOCKING);
            std::vector<std::complex<short> >* intermediate = (std::vector<std::complex<short> >*) &(tmp->dataBuffer);
            // do work here
            std::vector<short>* output = (std::vector<short>*) intermediate;
            myOutput->pushPacket(*output, tmp->T, tmp->EOS, tmp->streamID);

        Interactions with non-BULKIO ports are left up to the component developer's discretion

    Properties:
        
        Properties are accessed directly as member variables. For example, if the
        property name is "baudRate", it may be accessed within member functions as
        "baudRate". Unnamed properties are given the property id as its name.
        Property types are mapped to the nearest C++ type, (e.g. "string" becomes
        "std::string"). All generated properties are declared in the base class
        (AGC_base).
    
        Simple sequence properties are mapped to "std::vector" of the simple type.
        Struct properties, if used, are mapped to C++ structs defined in the
        generated file "struct_props.h". Field names are taken from the name in
        the properties file; if no name is given, a generated name of the form
        "field_n" is used, where "n" is the ordinal number of the field.
        
        Example:
            // This example makes use of the following Properties:
            //  - A float value called scaleValue
            //  - A boolean called scaleInput
              
            if (scaleInput) {
                dataOut[i] = dataIn[i] * scaleValue;
            } else {
                dataOut[i] = dataIn[i];
            }
            
        Callback methods can be associated with a property so that the methods are
        called each time the property value changes.  This is done by calling 
        addPropertyChangeListener(<property name>, this, &AGC_i::<callback method>)
        in the constructor.

        Callback methods should take two arguments, both const pointers to the value
        type (e.g., "const float *"), and return void.

        Example:
            // This example makes use of the following Properties:
            //  - A float value called scaleValue
            
        //Add to AGC.cpp
        AGC_i::AGC_i(const char *uuid, const char *label) :
            AGC_base(uuid, label)
        {
            addPropertyChangeListener("scaleValue", this, &AGC_i::scaleChanged);
        }

        void AGC_i::scaleChanged(const float *oldValue, const float *newValue)
        {
            std::cout << "scaleValue changed from" << *oldValue << " to " << *newValue
                      << std::endl;
        }
            
        //Add to AGC.h
        void scaleChanged(const float* oldValue, const float* newValue);
        

************************************************************************************************/
int AGC_i::serviceFunction()
{
	bulkio::InFloatPort::dataTransfer *tmp = dataFloat_in->getPacket(bulkio::Const::BLOCKING);
	if(not tmp)
		return NOOP;
	boost::mutex::scoped_lock lock(propLock_);

	{
		if(tmp->sriChanged){
			m_delta = tmp->SRI.xdelta;
			createAGC_Obj();
			//push new SRI data
			m_sriOut = tmp->SRI;
			m_sriOut.mode = tmp->SRI.mode;
			dataFloat_out->pushSRI(m_sriOut);
			//std::cout<<"updated SRI"<<std::endl;
		}

		//agc_crcf_reset(m_agc);
		if(enable){
			//std::cout<<"beginning processing"<<std::endl;
			if(tmp->SRI.mode==0)
				procReal(tmp->dataBuffer);
			else{
				std::vector<std::complex<float> >* cmp = (std::vector<std::complex<float> >*) &(tmp->dataBuffer);
				procComplex(*cmp);
			}
		}
		else
			output = &(tmp->dataBuffer);

	}
	dataFloat_out->pushPacket(*output, tmp->T, tmp->EOS, tmp->streamID);
	delete tmp;
	return NORMAL;
}

void AGC_i::createAGC_Obj()
{
	if(m_complex_agc){
		agc_crcf_destroy(m_complex_agc);
		m_complex_agc = NULL;
	}
	m_complex_agc = agc_crcf_create();
	agc_crcf_set_bandwidth(m_complex_agc, bandwidth);
	//agc_crcf_set_gain_limits(m_complex_agc,minPower,maxPower);

	if(m_real_agc){
		agc_rrrf_destroy(m_real_agc);
		m_real_agc = NULL;
	}
	m_real_agc = agc_rrrf_create();
	agc_rrrf_set_bandwidth(m_real_agc, bandwidth);
	//agc_rrrf_set_gain_limits(m_real_agc,minPower,maxPower);
}
