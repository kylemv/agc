#include <iostream>
#include "ossie/ossieSupport.h"

#include "AGC.h"
int main(int argc, char* argv[])
{
    AGC_i* AGC_servant;
    Resource_impl::start_component(AGC_servant, argc, argv);
    return 0;
}

