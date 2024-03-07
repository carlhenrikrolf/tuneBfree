/*
 * Microtuning functions
 */

// #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
// #include "doctest.h"
#include "libMTSClient.h"

void getFrequencies(double *frequency)
{
    MTSClient *client = MTS_RegisterClient();
    int i;
    for (i = 0; i <= 127; i++)
    {
        frequency[i] = MTS_NoteToFrequency(client, i, 0);
    }
    MTS_DeregisterClient(client);
}
