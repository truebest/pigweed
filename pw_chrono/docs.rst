.. _module-pw_chrono:

=========
pw_chrono
=========
.. pigweed-module::
   :name: pw_chrono

Pigweed's chrono module provides facilities for applications to deal with time,
leveraging many pieces of STL's the ``std::chrono`` library but with a focus
on portability for constrained embedded devices and maintaining correctness.

-------------------------------
``duration`` and ``time_point``
-------------------------------
Pigweed's time primitives rely on C++'s
`<chrono> <https://en.cppreference.com/w/cpp/header/chrono>`_ library to enable
users to express intents with strongly typed real time units through
`std::chrono::duration <https://en.cppreference.com/w/cpp/chrono/duration>`_
and
`std::chrono::time_point <https://en.cppreference.com/w/cpp/chrono/time_point>`_
.

What are they?
==============
At a high level, durations and time_points at run time are tick counts which
are wrapped in templated metadata which is only used at compile time.

The STL's
`std::chrono::duration <https://en.cppreference.com/w/cpp/chrono/duration>`_
class template represents a time interval. It consists of a count of ticks of
type ``rep`` and a tick ``period``, where the tick period is a ``std::ratio``
representing the time in seconds from one tick to the next.

The only data stored in a duration is a tick count of type ``rep``. The
``period`` is included as part of the duration's type, and is only used when
converting between different durations.

Similarly, the STL's
`std::chrono::time_point <https://en.cppreference.com/w/cpp/chrono/time_point>`_
class template represents a point in time (i.e. timestamp). It consists of a
value of type ``duration`` which represents the time interval from the start of
the ``clock``'s epoch.

The ``duration`` and ``time_point`` class templates can be represented with the
following simplified model, ignoring most of their member functions:

.. code-block:: cpp

   namespace std::chrono {

   template<class Rep, class Period = std::ratio<1, 1>>
   class duration {
    public:
     using rep = Rep;
     using period = Period;

     constexpr rep count() const { return tick_count_; }

     static constexpr duration zero() noexcept {
       return duration(0);
     }

     // Other member functions...

    private:
     rep tick_count_;
   };

   template<class Clock, class Duration = typename Clock::duration>
   class time_point {
    public:
     using duration = Duration;
     using rep = Duration::rep;
     using period = Duration::period;
     using clock = Clock;

     constexpr duration time_since_epoch() const { return time_since_epoch_; }

     // Other member functions...

    private:
     duration time_since_epoch_;
   };

  }  // namespace std::chrono

What ``rep`` type should be used?
=================================
The duration's ``rep``, or tick count type, can be a floating point or a signed
integer. For most applications, this is a signed integer just as how one may
represent the number of ticks for an RTOS API or the number of nanoseconds in
POSIX.

The ``rep`` should be able to represent the durations of time necessary for the
application. When possible, use ``int64_t`` as the ``rep`` for a clock's
duration in order to trivially avoid integer underflow and overflow risks by
covering a range of at least ±292 years. This matches the STL's requirements
for the duration helper types which are relevant for a clock's tick period:

* ``std::chrono::nanoseconds  duration</*signed integer type of at least 64 bits*/, std::nano>``
* ``std::chrono::microseconds duration</*signed integer type of at least 55 bits*/, std::micro>``
* ``std::chrono::milliseconds duration</*signed integer type of at least 45 bits*/, std::milli>``
* ``std::chrono::seconds  duration</*signed integer type of at least 35 bits*/>``

With this guidance one can avoid common pitfalls like ``uint32_t`` millisecond
tick rollover bugs when using RTOSes every 49.7 days.

.. warning::
   Avoid the ``duration<>::min()`` and ``duration<>::max()`` helper member
   functions where possible as they exceed the ±292 years duration limit
   assumption. There's an immediate risk of integer underflow or overflow for
   any arithmetic operations. Consider using ``std::optional`` instead of
   priming a variable with a value at the limit.

Helper duration types and literals
==================================
The STL's ``<chrono>`` library includes a set of helper types based on actual
time units, including the following (and more):

* ``std::chrono::nanoseconds``
* ``std::chrono::microseconds``
* ``std::chrono::milliseconds``
* ``std::chrono::seconds``
* ``std::chrono::minutes``
* ``std::chrono::hours``

As an example you can use these as follows:

.. code-block:: cpp

   #include <chrono>

   void Foo() {
     Bar(std::chrono::milliseconds(42));
   }

In addition, the inline namespace ``std::literals::chrono_literals`` includes:

* ``operator""ns`` for ``std::chrono::nanoseconds``
* ``operator""us`` for ``std::chrono::microseconds``
* ``operator""ms`` for ``std::chrono::milliseconds``
* ``operator""s`` for ``std::chrono::seconds``
* ``operator""min`` for ``std::chrono::minutes``
* ``operator""h`` for ``std::chrono::hours``

As an example you can use these as follows:

.. code-block:: cpp

   using std::literals::chrono_literals::ms;
   // Or if you want them all: using namespace std::chrono_literals;

   void Foo() {
     Bar(42ms);
   }

For these helper duration types to be compatible with API's that take a
`SystemClock::duration` either an :ref:`implicit<Implicit lossless conversions>`
or :ref:`explicit lossy<Explicit lossy conversions>` conversion must be done.

Converting between time units and clock durations
=================================================
So why go through all of this trouble instead of just using ticks or instead
just using one time unit such as nanoseconds? For example, imagine that you have
a 1kHz RTOS tick period and you would like to express a timeout duration:

.. code-block:: cpp

   // Instead of using ticks which are not portable between RTOS configurations,
   // as the tick period may be different:
   constexpr uint32_t kFooNotificationTimeoutTicks = 42;
   bool TryGetNotificationFor(uint32_t ticks);

   // And instead of using a time unit which is prone to accidental conversion
   // errors as all variables must maintain the time units:
   constexpr uint32_t kFooNotificationTimeoutMs = 42;
   bool TryGetNotificationFor(uint32_t milliseconds);

   // We can instead use a defined clock and its duration for the kernel and rely
   // on implicit lossless conversions:
   #include <chrono>
   #include "pw_chrono/system_clock.h"
   constexpr SystemClock::duration kFooNotificationTimeout =
       std::chrono::milliseconds(42);
   bool TryGetNotificationFor(SystemClock::duration timeout);

   void MaybeProcessNotification() {
     if (TryGetNotificationFor(kFooNotificationTimeout)) {
       ProcessNotification();
     }
   }

.. _Implicit lossless conversions:

Implicit lossless conversions
-----------------------------
Wait, but how does this work? Is there a hidden cost? The ``duration`` type
comes with built in implicit lossless conversion support which is evaluated at
compile time where possible.

If you rely on implicit conversions then the worst case cost is multiplication,
there is no risk of a division operation.

If the implicit conversion cannot be guaranteed at compile time to be lossless
for all possible tick count values, then it will fail to compile.

As an example you can always convert from ``std::chrono::seconds`` to
``std::chrono::milliseconds`` in a lossless manner. However, you cannot
guarantee for all tick count values that ``std::chrono::milliseconds`` can be
losslessly converted to ``std::chrono::seconds``, even though it may work for
some values like ``0``, ``1000``, etc.

.. code-block:: cpp

   #include <chrono>

   constexpr std::chrono::milliseconds this_compiles =
       std::chrono::seconds(42);

   // This cannot compile, because for some duration values it is lossy even
   // though this particular value can be in theory converted to whole seconds.
   // constexpr std::chrono::seconds this_does_not_compile =
   //    std::chrono::milliseconds(1000);

.. _Explicit lossy conversions:

Explicit lossy conversions
--------------------------
While code should prefer implicit lossless conversions whenever possible,
sometimes a lossy conversion is required.

Consider an example where a RTOS employs a 128Hz tick clock. The 128Hz
``period`` can be perfectly represented with a ``std::ratio<1,128>``. However
you will not be able to implicitly convert any real time unit durations to this
duration type. Instead explicit lossy conversions must be used. Pigweed
recommends explicitly using:

* `std::chrono::floor <https://en.cppreference.com/w/cpp/chrono/duration/floor>`_
  to round down.
* `std::chrono::round <https://en.cppreference.com/w/cpp/chrono/duration/round>`_
  to round to the nearest, rounding to even in halfway cases.
* `std::chrono::ceil <https://en.cppreference.com/w/cpp/chrono/duration/ceil>`_
  to round up.
* `pw::chrono::SystemClock::for_at_least` to round up using the `SystemClock::period`,
  as a more explicit form of std::chrono::ceil.

.. Note::
   Pigweed does not recommend using ``std::chrono::duration_cast<>`` which
   truncates dowards zero like ``static_cast``. This is typically not the desired
   rounding behavior when dealing with time units. Instead, where possible we
   recommend the more explicit, self-documenting ``std::chrono::floor``,
   ``std::chrono::round``, and ``std::chrono::ceil``.

Now knowing this, the previous example could be portably and correctly handled
as follows:

.. code-block:: cpp

   #include <chrono>

   #include "pw_chrono/system_clock.h"

   // We want to round up to ensure we block for at least the specified duration,
   // instead of rounding down. Imagine for example the extreme case where you
   // may round down to zero or one, you would definitely want to at least block.
   constexpr SystemClock::duration kFooNotificationTimeout =
       std::chrono::ceil(std::chrono::milliseconds(42));
   bool TryGetNotificationFor(SystemClock::duration timeout);

   void MaybeProcessNotification() {
     if (TryGetNotificationFor(kFooNotificationTimeout)) {
       ProcessNotification();
     }
   }

This code is lossless if the clock period is 1kHz and it's correct using a
division which rounds up when the clock period is 128Hz.

.. Note::
   When using ``pw::chrono::SystemClock::duration`` for timeouts, prefer
   using its ``SystemClock::for_at_least()`` to round up timeouts in a more
   explicit, self documenting manner which uses ``std::chrono::ceil``
   internally.

Use of ``count()`` and ``time_since_epoch()``
=============================================
It's easy to escape the typesafe chrono types through the use of
``duration<>::count()`` and ``time_point<>::time_since_epoch()``, however this
increases the risk of accidentally introduce conversion and arithmetic errors.

For this reason, avoid these two escape hatches until it's absolutely necessary
due to I/O such as RPCs or writing to non-volatile storage.

Discrete Timeouts
=================
We briefly want to mention a common pitfall when working with discrete
representations of time durations for timeouts (ticks and real time units)
on systems with a continously running clock which is backed by discrete time
intervals (i.e. whole integer constant tick periods).

Imagine an RTOS system where we have a constant tick interval. If we attempt to
sleep for 1 tick, how long will the kernel actually let us sleep?

In most kernels you will end up sleeping somewhere between 0 and 1 tick periods
inclusively, i.e. ``[0, 1]``, if we ignore scheduling latency and preemption.
**This means it can randomly be non-blocking vs blocking!**

This is because internally kernels use a decrementing timeout counter or a
deadline without taking the current current progression through the existing
tick period into account.

For this reason all of Pigweed's time bound APIs will internally add an extra
tick to timeout intents when needed to guarantee that we will block for at least
the specified timeout.

This same risk exists if a continuously running hardware timer is used for a
software timer service.

.. Note::
   When calculating deadlines based on a ``pw::chrono::SystemClock::timeout``,
   use ``SystemClock::TimePointAfterAtLeast()`` which adds an extra tick for you
   internally.

------
Clocks
------
We do not recomend using the clocks provided by ``<chrono>`` including but not
limited to the ``std::chrono::system_clock``, ``std::chrono::steady_clock``, and
``std::chrono::high_resolution_clock``. These clocks typically do not work on
embedded systems, as they are not backed by any actual clocks although they
often do compile. In addition, their APIs miss guarantees and parameters which
make them difficult and risky to use on embedded systems.

In addition, the STL time bound APIs heavily rely on templating to permit
different clocks and durations to be used. We believe this level of template
metaprogramming and the indirection that comes with that can be confusing. On
top of this, accidental use of the wrong clock and/or conversions between them
is a frequent source of bugs. For example using a real time clock which is not
monotonic for a timeout or deadline can wreak havoc when the clock is adjusted.

For this reason Pigweed's timeout and deadline APIs will not permit arbitrary
clock and duration selection. Outside of small templated helpers, all APIs will
require a specific clock's duration and/or time-point. For almost all of Pigweed
this means that the ``pw::chrono::SystemClock`` is used which is usually backed
by the kernel's clock.

PigweedClock Requirements
=========================
``pw_chrono`` extends the C++ named
`Clock <https://en.cppreference.com/w/cpp/named_req/Clock>`_ and
`TrivialClock <https://en.cppreference.com/w/cpp/named_req/TrivialClock>`_
requirements with the ``PigweedClock Requirements`` to make clocks more friendly
for embedded systems.

This permits the clock compatibility to be verified through ``static_assert`` at
compile time which the STL's requirements do not address. For example whether
the clock continues to tick while interrupts are masked or whether the clock is
monotonic even if the clock period may not be steady due to the use of low power
sleep modes.

For a type ``PWC`` to meet the ``PigweedClock Requirements``:

* The type PWC must meet C++14's
  `Clock <https://en.cppreference.com/w/cpp/named_req/Clock>`_ and
  `TrivialClock <https://en.cppreference.com/w/cpp/named_req/TrivialClock>`_
  requirements.
* The ``PWC::rep`` must be ``int64_t`` to ensure that there cannot be any
  overflow risk regardless of the ``PWC::period`` configuration.
  This is done because we do not expect any clocks with periods coarser than
  seconds which already require 35 bits.
* ``const bool PWC::is_monotonic`` must return true if and only if the clock
  can never move backwards.
  This effectively allows one to describe an unsteady but monotonic clock by
  combining the C++14's Clock requirement's ``const bool PWC::is_steady``.
* ``const bool PWC::is_free_running`` must return true if and only if the clock
  continues to move forward, without risk of overflow, regardless of whether
  global interrupts are disabled or whether one is in a critical section or even
  non maskable interrupt.
* ``const bool PWC::is_always_enabled`` must return true if the clock is always
  enabled and available. If false, the clock must:

  + Ensure the ``const bool is_{steady,monotonic,free_running}`` attributes
    are all valid while the clock is not enabled to ensure they properly meet
    the previously stated requirements.
  + Meet C++14's
    `BasicLockable <https://en.cppreference.com/w/cpp/named_req/BasicLockable>`_
    requirements (i.e. provide ``void lock()`` & ``void unlock()``) in order
    to provide ``std::scoped_lock`` support to enable a user to enable the
    clock.
  + Provide ``const bool is_{steady,monotonic,free_running}_while_enabled``
    attributes which meet the attributes only while the clock is enabled.
* ``const bool PWC::is_stopped_in_halting_debug_mode`` must return true if and
  only if the clock halts, without further modification, during halting debug
  mode , for example during a breakpoint while a hardware debugger is used.
* ``const Epoch PWC::epoch`` must return the epoch type of the clock, the
  ``Epoch`` enumeration is defined in ``pw_chrono/epoch.h``.
* The function ``time_point PWC::now() noexcept`` must always be thread and
  interrupt safe, but not necessarily non-masking and bare-metal interrupt safe.
* ``const bool PWC::is_non_masking_interrupt_safe`` must return true if and only
  if the clock is safe to use from non-masking and bare-metal interrupts.

The PigweedClock requirement will not require ``now()`` to be a static function,
however the upstream façades will follow this approach.

SystemClock facade
==================
The ``pw::chrono::SystemClock`` is meant to serve as the clock used for time
bound operations such as thread sleeping, waiting on mutexes/semaphores, etc.
The ``SystemClock`` always uses a signed 64 bit as the underlying type for time
points and durations. This means users do not have to worry about clock overflow
risk as long as rational durations and time points as used, i.e. within a range
of ±292 years.

The ``SystemClock`` represents an unsteady, monotonic clock.

The epoch of this clock is unspecified and may not be related to wall time
(for example, it can be time since boot). The time between ticks of this
clock may vary due to sleep modes and potential interrupt handling.
``SystemClock`` meets the requirements of C++'s ``TrivialClock`` and Pigweed's
``PigweedClock``.

This clock is used for expressing timeout and deadline semantics with the
scheduler in Pigweed including pw_sync, pw_thread, etc.

C++
---
.. doxygenstruct:: pw::chrono::SystemClock
   :members:

Example in C++
--------------
.. code-block:: cpp

   #include <chrono>

   #include "pw_chrono/system_clock.h"

   void Foo() {
     const SystemClock::time_point before = SystemClock::now();
     TakesALongTime();
     const SystemClock::duration time_taken = SystemClock::now() - before;
     bool took_way_too_long = false;
     if (time_taken > std::chrono::seconds(42)) {
       took_way_too_long = true;
     }
   }

VirtualClock
============
Pigweed also includes a virtual base class for timers,
:cpp:class:`pw::chrono::VirtualClock`. This class allows for writing
timing-sensitive code that can be tested using simulated clocks such as
:cpp:class:`pw::chrono::SimulatedSystemClock`.

Using simulated clocks in tests allow tests to avoid sleeping or timeouts,
resulting in faster and more reliable tests.

See also :cpp:class:`pw::async2::TimeProvider` for creating testable
time-sensitive code using asynchronous timers.

.. doxygenclass:: pw::chrono::VirtualClock
  :members:

.. doxygenclass:: pw::chrono::SimulatedSystemClock
  :members:



Protobuf
========
Sometimes it's desirable to communicate high resolution time points and
durations from one device to another. For this, ``pw_chrono`` provides protobuf
representations of clock parameters (``pw.chrono.ClockParameters``) and time
points (``pw.chrono.TimePoint``). These types are less succinct than simple
single-purpose fields like ``ms_since_boot`` or ``unix_timestamp``, but allow
timestamps to be communicated in terms of the tick rate of a device, potentially
providing significantly higher resolution. Logging, tracing, and system state
snapshots are use cases that benefit from this additional resolution.

This module provides an overlay proto (``pw.chrono.SnapshotTimestamps``) for
usage with ``pw_snapshot`` to encourage capture of high resolution timestamps
in device snapshots. Simplified capture utilies and host-side tooling to
interpret this data are not yet provided by ``pw_chrono``.

There is tooling that take these proto and make them more human readable.

---------------
Software Timers
---------------

SystemTimer facade
==================
The SystemTimer facade enables deferring execution of a callback until a later
time. For example, enabling low power mode after a period of inactivity.

The base SystemTimer only supports a one-shot style timer with a callback.
A periodic timer can be implemented by rescheduling the timer in the callback
through ``InvokeAt(kDesiredPeriod + expired_deadline)``.

When implementing a periodic layer on top, the user should be mindful of
handling missed periodic callbacks. They could opt to invoke the callback
multiple times with the expected ``expired_deadline`` values or instead saturate
and invoke the callback only once with the latest ``expired_deadline``.

The entire API is thread safe, however it is NOT always IRQ safe.

The ExpiryCallback is either invoked from a high priority thread or an
interrupt. Ergo ExpiryCallbacks should be treated as if they are executed by an
interrupt, meaning:

* Processing inside of the callback should be kept to a minimum.

* Callbacks should never attempt to block.

* APIs which are not interrupt safe such as pw::sync::Mutex should not be used!

C++
---
.. doxygenclass:: pw::chrono::SystemTimer
   :members:

.. cpp:namespace-push:: pw::chrono::SystemTimer

.. list-table::
   :widths: 70 10 10 10
   :header-rows: 1

   * - Safe to use in context
     - Thread
     - Interrupt
     - NMI
   * - :cpp:func:`pw::chrono::SystemTimer::SystemTimer`
     - ✔
     -
     -
   * - :cpp:func:`pw::chrono::SystemTimer::~SystemTimer`
     - ✔
     -
     -
   * - :cpp:func:`InvokeAfter`
     - ✔
     -
     -
   * - :cpp:func:`InvokeAt`
     - ✔
     -
     -
   * - :cpp:func:`Cancel`
     - ✔
     -
     -

.. cpp:namespace-pop::

Example in C++
--------------
.. code-block:: cpp

   #include "pw_chrono/system_clock.h"
   #include "pw_chrono/system_timer.h"
   #include "pw_log/log.h"

   using namespace std::chrono_literals;

   void DoFoo(pw::chrono::SystemClock::time_point expired_deadline) {
     PW_LOG_INFO("Timer callback invoked!");
   }

   pw::chrono::SystemTimer foo_timer(DoFoo);

   void DoFooLater() {
     foo_timer.InvokeAfter(42ms);  // DoFoo will be invoked after 42ms.
   }

.. _module-pw_chrono-libc-time-wrappers:

------------------
libc time wrappers
------------------
The `gettimeofday <https://pubs.opengroup.org/onlinepubs/9699919799/functions/gettimeofday.html>`__
and `time <https://pubs.opengroup.org/onlinepubs/9699919799/functions/time.html>`__
POSIX functions are defined to return the current time since the Epoch.
The default ``pw_toolchain/arg_gcc:newlib_os_interface_stubs`` stub for
``gettimeofday`` will cause a linker error if any code tried to use this
function, but it's common for software not written for embedded systems to
depend on this function being defined and returning something that increments
like a clock. In addition, some software depends on having ``gettimeofday``
return something much closer to the actual time so it can compare against well
known time points inside TLS certificates for instance.

For compatibility with such software, ``pw_toolchain`` provides two different
options to wrap libc time functions. Both of these are not recommended for
general time querying and are only intended to provide compatibility.

``pw_chrono:wrap_time_build_time``
==================================
Wrap ``gettimeofday`` and ``time`` with an implementation that returns a static
time value at which the library was built. Use this option if you need these
functions to return a known value greater than some point in the past.

.. note::
   When building with Bazel, use the `--stamp
   <https://bazel.build/docs/user-manual#stamp>`__ flag when building release
   binaries to ensure the build time reflects the actual time the build is
   executed, as opposed to a cached value.

``pw_chrono:wrap_time_system_clock``
====================================
Wrap ``gettimeofday`` and ``time`` with an implementation that uses
``pw::chrono::SystemClock`` to return the current time. Note the epoch is
determined by the SystemClock backend epoch, which on most embedded systems will
be time since boot. Use this option if you don't care about the time returned
being close to actual time, but do care that it increments like a real clock.

.. toctree::
   :hidden:
   :maxdepth: 1

   backends
