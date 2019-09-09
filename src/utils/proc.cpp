#include <ostream>
#include <fstream>

#include "proc.h"

#include "utils.h"

namespace proc {

std::ostream& operator<< (std::ostream &os, const PrettySize &s) {
  std::string unit;
  auto scaled_size = prettySize(s.size, unit);
  return os << scaled_size << unit;
}

int prettySize (unsigned long long int size, std::string &unit) {
  if (size > 1024) {
    size = std::round(size / 1024);
    unit = "Ki";
  }
  if (size > 1024) {
    size = std::round(size / 1024);
    unit = "Mi";
  }
  if (size > 1024) {
    size = std::round(size / 1024);
    unit = "Gi";
  }
  if (size > 1024) {
    size = std::round(size / 1024);
    unit = "Ti";
  }

  unit += "B";
  return size;
}

long int pagesize (void) {
  static const long int value = sysconf(_SC_PAGE_SIZE);
  return value;
}

Stat Stat::fetch (void) {
  Stat pstats;
  std::ifstream sstat (statsfile, std::ios_base::in);
  if (!sstat)
    utils::doThrow<std::invalid_argument>("Unable to open ", statsfile);

  sstat >> pstats.pid >> pstats.comm >> pstats.state >> pstats.ppid
        >> pstats.pgrp >> pstats.session >> pstats.tty_nr >> pstats.tpgid
        >> pstats.flags >> pstats.minflt >> pstats.cminflt >> pstats.majflt
        >> pstats.cmajflt >> pstats.utime >> pstats.stime >> pstats.cutime
        >> pstats.cstime >> pstats.priority >> pstats.nice
        >> pstats.num_threads >> pstats.itrealvalue >> pstats.starttime
        >> pstats.vsize >> pstats.rss >> pstats.rsslim >> pstats.startcode
        >> pstats.endcode >> pstats.startstack >> pstats.kstkesp
        >> pstats.kstkeip >> pstats.signal >> pstats.blocked
        >> pstats.sigignore >> pstats.sigcatch >> pstats.wchan
        >> pstats.nswap >> pstats.cnswap >> pstats.exit_signal
        >> pstats.processor >> pstats.rt_priority >> pstats.policy
        >> pstats.delayacct_blkio_ticks >> pstats.guest_time
        >> pstats.cguest_time >> pstats.start_data >> pstats.end_data
        >> pstats.start_brk >> pstats.arg_start >> pstats.arg_end
        >> pstats.env_start >> pstats.env_end >> pstats.exit_code;

  pstats.rss *= pagesize();

  return pstats;
}

Statm Statm::fetch (void) {
  Statm mstats;
  std::ifstream sstatm (statsfile, std::ios_base::in);
  if (!sstatm)
    utils::doThrow<std::invalid_argument>("Unable to open ", statsfile);

  sstatm >> mstats.size >> mstats.resident >> mstats.shared
         >> mstats.text
//         >> mstats.lib
         >> mstats.data
//         >> mstats.dt
      ;

  mstats.size *= pagesize();
  mstats.resident *= pagesize();
  mstats.shared *= pagesize();
  mstats.text *= pagesize();
  mstats.data *= pagesize();

  return mstats;
}

void assert_lighter_than(long int size_limit) {
  auto resident = Statm::fetch().resident;
  if (size_limit < resident) {
    std::string unit, lunit;
    auto size = prettySize(resident, unit);
    auto lsize = prettySize(size_limit, lunit);
    utils::doThrow<std::out_of_range>(
      "Program size (", size, unit, ") is greater than allowed max (",
      lsize, lunit, ")");
  }
}

} // end of namespace proc
