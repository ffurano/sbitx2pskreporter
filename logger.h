#ifndef Logger_HH
#define Logger_HH

#include <syslog.h>
#include <pthread.h>

#include <sstream>
#include <string>

#include <map>
#include <vector>


// make wrapper for strerror_r function to overcome
// _GNU_SOURCE (glibc) vs. pure POSIX implementation
#ifdef _GNU_SOURCE
#define dpm_strerror_r(errnum, buf, buflen) \
  if (buflen > 0) { \
    int old_errno = errno, cur_errno = errnum; \
    char buffer[128], *msg = NULL; \
    errno = 0; \
    buf[0] = '\0'; \
    msg = strerror_r(cur_errno, buffer, sizeof(buffer)); \
    if (msg) \
      strncpy(buf, msg, buflen); \
    else \
      snprintf(buf, buflen, "Unknown error %d", errnum); \
    buf[buflen-1] = '\0'; \
    errno = old_errno; \
  }
#else
#define dpm_strerror_r(errnum, buf, buflen) \
  if (buflen > 0) { \
    int old_errno = errno, cur_errno = errnum, rc; \
    errno = 0; \
    rc = strerror_r(cur_errno, buf, buflen); \
    switch (rc) { \
      case 0: \
      case ERANGE: \
        break; \
      case EINVAL: \
        snprintf(buf, buflen, "Unknown error %d", errnum); \
        buf[buflen-1] = '\0'; \
        break; \
    } \
    errno = old_errno; \
  }
#endif


#define SSTR(message) static_cast<std::ostringstream&>(std::ostringstream().flush() << message).str()

#define Log(lvl, mymask, where, what) 												\
do{                                											\
	if (Logger::get()->getLevel() >= lvl && Logger::get()->isLogged(mymask)) 	\
	{    																	\
		std::ostringstream outs;                                   			\
        outs << "{" << pthread_self() << "}" << "[" << lvl << "] " << where << " " << __func__ << " : " << what;                      			\
		Logger::get()->log((Logger::Level)lvl, outs.str());    				\
	}                                                             			\
}while(0)                                                               			\


#define Err(where, what) 												\
do{                                											\
		std::ostringstream outs;                                   			\
        outs << "{" << pthread_self() << "}" << "!!! " << where << " " << __func__ << " : " << what;                      			\
		Logger::get()->log((Logger::Level)0, outs.str());    				\
}while(0)

/**
 * A Logger class
 */
class Logger
{

public:
	/// typedef for a bitmask (long long)
	typedef unsigned long long bitmask;
	/// typedef for a component name (std:string)
	typedef std::string component;

	static bitmask unregistered;
        static char *unregisteredname;
    /**
     * Use the same values for log levels as syslog
     */
    enum Level
    {
        Lvl0,       // The default?
        Lvl1,
        Lvl2,
        Lvl3,
        Lvl4,
        Lvl5
    };

    /// Destructor
    ~Logger();

    static Logger *instance;

    /// @return the singleton instance
    static Logger *get()
    {
    	if (instance == 0)
	  instance = new Logger();
    	return instance;
    }

    static void set(Logger *inst) {
      Logger *old = instance;
      instance = inst;
      if (old) delete old;
    }
    /// @return the current debug level
    short getLevel() const
    {
        return level;
    }

    /// @param lvl : the logging level that will be set
    void setLevel(Level lvl)
    {
        level = lvl;
    }

    /// @return true if the given component is being logged, false otherwise
    bool isLogged(bitmask m) const
    {
        if (mask == 0) return mask & unregistered;
    	return mask & m;
    }

    /// @param comp : the component that will be registered for logging
    void registerComponent(component const &  comp);

    /// @param components : list of components that will be registered for logging
    void registerComponents(std::vector<component> const & components);

    /// Sets if a component has to be logged or not
    /// @param comp : the component name
    /// @param tobelogged : true if we want to log this component
    void setLogged(component const &comp, bool tobelogged);

    /**
     * Logs the message
     *
     * @param lvl : log level of the message
     * @param component : bitmask assignet to the given component
     * @param msg : the message to be logged
     */
    void log(Level lvl, std::string const & msg) const;

    /**
     * @param if true all unregistered components will be logged,
     * 			if false only registered components will be logged
     */
    void logAll()
    {
    	mask = ~0;
    }


    /**
     * @param comp : component name
     * @return respectiv bitmask assigned to given component
     */
    bitmask getMask(component const & comp);

    /**
     * Build a printable stacktrace. Useful e.g. inside exceptions, to understand
     * where they come from.
     * Note: I don't think that the backtrace() function is thread safe, nor this function
     * Returns the number of backtraces
     * @param s : the string that will contain the printable stacktrace
     * @return the number of stacktraces
     */
    static int getStackTrace(std::string &s);

private:

    ///Private constructor
    Logger();
    // Copy constructor (not implemented)
    Logger(Logger const &);
    // Assignment operator (not implemented)
    Logger & operator=(Logger const &);

    /// current log level
    short level;
    /// number of components that were assigned with a bitmask
    int size;
    /// global bitmask with all registered components
    bitmask mask;
    /// component name to bitmask mapping
    std::map<component, bitmask> mapping;



};


// Specialized func to log configuration values. Filters out sensitive stuff.
void LogCfgParm(int lvl, Logger::bitmask mymask, std::string where, std::string key, std::string value);




#endif
