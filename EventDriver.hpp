#ifndef EVENTDRIVER_HPP
#define EVENTDRIVER_HPP

class EventDriver {
public:
  
    virtual int  startPoll() = 0;
    virtual bool addRead(int) = 0;
    virtual bool addWrite(int)   = 0;
    virtual bool addExcept(int) = 0;
  
    virtual bool deleteRead(int) = 0;
    virtual bool deleteWrite(int)  = 0;
    virtual bool deleteExcept(int) = 0;
  
    virtual int eventRead(int)   = 0;
    virtual int eventWrite(int)  = 0;
    virtual int eventExcept(int) = 0;
  
    virtual ~EventDriver(){};
};

#endif
