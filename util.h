#include <Arduino.h>

class DevNull: public Print
{
public:
    virtual size_t write(uint8_t) { return 1; }
};

#ifdef NDEBUG
DevNull devnull;
#define DBG devnull
#define NSTATUS
#elif defined NSTATUS
DevNull devnull;
#define DBG Serial
#else
#define DBG Serial
#endif
