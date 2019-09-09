#ifndef PROC_H
#define PROC_H

#include <unistd.h>

#include <string>
//#include <ostream>

namespace proc {

/// Scales the size to the nearest order of magnitude
/// Returns the unit in the provided string
int prettySize (unsigned long long int size, std::string &unit);

/// Wrapper to pretty print a size (in bytes) rounded to the nearest order
struct PrettySize {

  /// The size to format
  const unsigned long long int size;

  /// Constructor the get the size
  PrettySize (unsigned long long int size) : size(size) {}

  /// Pretty print the size
  friend std::ostream& operator<< (std::ostream &os, const PrettySize &s);
};

/// \return the linux page size
long int pagesize (void);

/// Extracted from `man 5 proc` under /proc/self/stat
struct Stat {

  /// The file to read for (way too much) processus-related data
  static constexpr auto statsfile = "/proc/self/stat";

  /// Return a filled-in stats structure according to the content of statsfile
  static Stat fetch (void);

  int pid; /*!< (1)
            The process ID.
  */

  std::string comm; /*!< (2)
            The filename of the executable, in parentheses.
            This is visible whether or not the executable is
            swapped out.
  */

  char state; /*!< (3)
            One of the following characters, indicating process
            state:

            R  Running

            S  Sleeping in an interruptible wait

            D  Waiting in uninterruptible disk sleep

            Z  Zombie

            T  Stopped (on a signal) or (before Linux 2.6.33)
                trace stopped

            t  Tracing stop (Linux 2.6.33 onward)

            W  Paging (only before Linux 2.6.0)

            X  Dead (from Linux 2.6.0 onward)

            x  Dead (Linux 2.6.33 to 3.13 only)

            K  Wakekill (Linux 2.6.33 to 3.13 only)

            W  Waking (Linux 2.6.33 to 3.13 only)

            P  Parked (Linux 3.9 to 3.13 only)
  */

  int ppid; /*!< (4)
            The PID of the parent of this process.
  */

  int pgrp; /*!< (5)
            The process group ID of the process.
  */

  int session; /*!< (6)
            The session ID of the process.
  */

  int tty_nr; /*!< (7)
            The controlling terminal of the process.  (The minor
            device number is contained in the combination of
            bits 31 to 20 and 7 to 0; the major device number is
            in bits 15 to 8.)
  */

  int tpgid; /*!< (8)
            The ID of the foreground process group of the con‐
            trolling terminal of the process.
  */

  unsigned int flags; /*!< (9)
            The kernel flags word of the process.  For bit mean‐
            ings, see the PF_* defines in the Linux kernel
            source file include/linux/sched.h.  Details depend
            on the kernel version.

            The format for this field was %lu before Linux 2.6.
  */

  unsigned long int minflt; /*!< (10)
            The number of minor faults the process has made
            which have not required loading a memory page from
            disk.
  */

  unsigned long int cminflt; /*!< (11)
            The number of minor faults that the process's
            waited-for children have made.
  */

  unsigned long int majflt; /*!< (12)
            The number of major faults the process has made
            which have required loading a memory page from disk.
  */

  unsigned long int cmajflt; /*!< (13)
            The number of major faults that the process's
            waited-for children have made.
  */

  unsigned long int utime; /*!< (14)
            Amount of time that this process has been scheduled
            in user mode, measured in clock ticks (divide by
            sysconf(_SC_CLK_TCK)).  This includes guest time,
            guest_time (time spent running a virtual CPU, see
            below), so that applications that are not aware of
            the guest time field do not lose that time from
            their calculations.
  */

  unsigned long int stime; /*!< (15)
            Amount of time that this process has been scheduled
            in kernel mode, measured in clock ticks (divide by
            sysconf(_SC_CLK_TCK)).
  */

  long int cutime; /*!< (16)
            Amount of time that this process's waited-for chil‐
            dren have been scheduled in user mode, measured in
            clock ticks (divide by sysconf(_SC_CLK_TCK)).  (See
            also times(2).)  This includes guest time,
            cguest_time (time spent running a virtual CPU, see
            below).
  */

  long int cstime; /*!< (17)
            Amount of time that this process's waited-for chil‐
            dren have been scheduled in kernel mode, measured in
            clock ticks (divide by sysconf(_SC_CLK_TCK)).
  */

  long int priority; /*!< (18)
            (Explanation for Linux 2.6) For processes running a
            real-time scheduling policy (policy below; see
            sched_setscheduler(2)), this is the negated schedul‐
            ing priority, minus one; that is, a number in the
            range -2 to -100, corresponding to real-time priori‐
            ties 1 to 99.  For processes running under a non-
            real-time scheduling policy, this is the raw nice
            value (setpriority(2)) as represented in the kernel.
            The kernel stores nice values as numbers in the
            range 0 (high) to 39 (low), corresponding to the
            user-visible nice range of -20 to 19.

            Before Linux 2.6, this was a scaled value based on
            the scheduler weighting given to this process.
  */

  long int nice; /*!< (19)
            The nice value (see setpriority(2)), a value in the
            range 19 (low priority) to -20 (high priority).
  */

  long int num_threads; /*!< (20)
            Number of threads in this process (since Linux 2.6).
            Before kernel 2.6, this field was hard coded to 0 as
            a placeholder for an earlier removed field.
  */

  long int itrealvalue; /*!< (21)
            The time in jiffies before the next SIGALRM is sent
            to the process due to an interval timer.  Since ker‐
            nel 2.6.17, this field is no longer maintained, and
            is hard coded as 0.
  */

  unsigned long long int starttime; /*!< (22)
            The time the process started after system boot.  In
            kernels before Linux 2.6, this value was expressed
            in jiffies.  Since Linux 2.6, the value is expressed
            in clock ticks (divide by sysconf(_SC_CLK_TCK)).

            The format for this field was %lu before Linux 2.6.
  */

  unsigned long int vsize; /*!< (23)
            Virtual memory size in bytes.
  */

  long int rss; /*!< (24)
            Resident Set Size: number of pages the process has
            in real memory.  This is just the pages which count
            toward text, data, or stack space.  This does not
            include pages which have not been demand-loaded in,
            or which are swapped out.
  */

  unsigned long int rsslim; /*!< (25)
            Current soft limit in bytes on the rss of the
            process; see the description of RLIMIT_RSS in
            getrlimit(2).
  */

  unsigned long int startcode; /*!< (26)  [PT]
            The address above which program text can run.
  */

  unsigned long int endcode; /*!< (27)  [PT]
            The address below which program text can run.
  */

  unsigned long int startstack; /*!< (28)  [PT]
            The address of the start (i.e., bottom) of the
            stack.
  */

  unsigned long int kstkesp; /*!< (29)  [PT]
            The current value of ESP (stack pointer), as found
            in the kernel stack page for the process.
  */

  unsigned long int kstkeip; /*!< (30)  [PT]
            The current EIP (instruction pointer).
  */

  unsigned long int signal; /*!< (31)
            The bitmap of pending signals, displayed as a deci‐
            mal number.  Obsolete, because it does not provide
            information on real-time signals; use
            /proc/[pid]/status instead.
  */

  unsigned long int blocked; /*!< (32)
            The bitmap of blocked signals, displayed as a deci‐
            mal number.  Obsolete, because it does not provide
            information on real-time signals; use
            /proc/[pid]/status instead.
  */

  unsigned long int sigignore; /*!< (33)
            The bitmap of ignored signals, displayed as a deci‐
            mal number.  Obsolete, because it does not provide
            information on real-time signals; use
            /proc/[pid]/status instead.
  */

  unsigned long int sigcatch; /*!< (34)
            The bitmap of caught signals, displayed as a decimal
            number.  Obsolete, because it does not provide
            information on real-time signals; use
            /proc/[pid]/status instead.
  */

  unsigned long int wchan; /*!< (35)  [PT]
            This is the "channel" in which the process is wait‐
            ing.  It is the address of a location in the kernel
            where the process is sleeping.  The corresponding
            symbolic name can be found in /proc/[pid]/wchan.
  */

  unsigned long int nswap; /*!< (36)
            Number of pages swapped (not maintained).
  */

  unsigned long int cnswap; /*!< (37)
            Cumulative nswap for child processes (not main‐
            tained).
  */

  int exit_signal; /*!< (38)  (since Linux 2.1.22)
            Signal to be sent to parent when we die.
  */

  int processor; /*!< (39)  (since Linux 2.2.8)
            CPU number last executed on.
  */

  unsigned int rt_priority; /*!< (40)  (since Linux 2.5.19)
            Real-time scheduling priority, a number in the range
            1 to 99 for processes scheduled under a real-time
            policy, or 0, for non-real-time processes (see
            sched_setscheduler(2)).
  */

  unsigned int policy; /*!< (41)  (since Linux 2.5.19)
            Scheduling policy (see sched_setscheduler(2)).
            Decode using the SCHED_* constants in linux/sched.h.

            The format for this field was %lu before Linux
            2.6.22.
  */

  unsigned long long int delayacct_blkio_ticks; /*!< (42)  (since Linux 2.6.18)
            Aggregated block I/O delays, measured in clock ticks
            (centiseconds).
  */

  unsigned long int guest_time; /*!< (43)  (since Linux 2.6.24)
            Guest time of the process (time spent running a vir‐
            tual CPU for a guest operating system), measured in
            clock ticks (divide by sysconf(_SC_CLK_TCK)).
  */

  long int cguest_time; /*!< (44)  (since Linux 2.6.24)
            Guest time of the process's children, measured in
            clock ticks (divide by sysconf(_SC_CLK_TCK)).
  */

  unsigned long int start_data; /*!< (45)  (since Linux 3.3)  [PT]
            Address above which program initialized and unini‐
            tialized (BSS) data are placed.
  */

  unsigned long int end_data; /*!< (46)  (since Linux 3.3)  [PT]
            Address below which program initialized and unini‐
            tialized (BSS) data are placed.
  */

  unsigned long int start_brk; /*!< (47)  (since Linux 3.3)  [PT]
            Address above which program heap can be expanded
            with brk(2).
  */

  unsigned long int arg_start; /*!< (48)  (since Linux 3.5)  [PT]
            Address above which program command-line arguments
            (argv) are placed.
  */

  unsigned long int arg_end; /*!< (49)  (since Linux 3.5)  [PT]
            Address below program command-line arguments (argv)
            are placed.
  */

  unsigned long int env_start; /*!< (50)  (since Linux 3.5)  [PT]
            Address above which program environment is placed.
  */

  unsigned long int env_end; /*!< (51)  (since Linux 3.5)  [PT]
            Address below which program environment is placed.
  */

  int exit_code; /*!< (52)  (since Linux 3.5)  [PT]
            The thread's exit status in the form reported by
            waitpid(2).
  */
};

/// Extracted from `man 5 proc` under /proc/self/statm
struct Statm {
  long int size;  ///< Total program size
  long int resident;  ///< Resident set size
  long int shared;  ///< Number of resident shared pages (backed by a file)

  long int text;  ///< Text (code)
//  long int lib;  ///< Library (unused since Lunix 2.6; always 0)
  long int data;  ///< data + stack
//  long int dt;  ///< dirty pages (unused since Lunix 2.6; always 0)

  /// The file to read for (way too much) processus-related data
  static constexpr auto statsfile = "/proc/self/statm";

  /// Return a filled-in stats structure according to the content of statsfile
  static Statm fetch (void);
};

void assert_lighter_than (long size_limit);

} // end of namespace proc

#endif // PROC_H
