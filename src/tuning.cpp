/*
 * Microtuning functions
 */

#ifdef TESTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#endif

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

#ifdef TESTS
TEST_CASE("Testing getFrequencies")
{
    double frequency[128] = {0.0};
    getFrequencies(frequency);
    double a = 32.70319566257483;
    double b = 5919.91076338615039;
    CHECK(frequency[24] == a);
    CHECK(frequency[24 + 12] == 2 * a);
    CHECK(frequency[114 - 12] == b / 2);
    CHECK(frequency[114] == b);
}
#endif
