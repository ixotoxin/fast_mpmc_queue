# Fast Lock-Free and Allocation-Free Multi-Producer/Multi-Consumer Queue

This is an implementation of a lock-free and allocation-free multi-producer/multi-consumer queue in C++. The queue
is implemented as an array of slots. Communication between producers and consumers occurs through writing and reading
information to/from these slots. The queue size is fixed and is specified as a template parameter. Under high contention
from a large number of worker threads, this can lead to starvation of some of them.  

Message order is not guaranteed, but the queue strives to preserve it.

## API

```c++
#include <xtxn/fastest_mpmc_queue.hpp>
```
Include the header file containing the template class declaration:

```c++
template<
    std::default_initializable T,
    int32_t S,
    bool C = true,
    int32_t A = queue_default_attempts
>
class fastest_mpmc_queue;
```
where
- `T` - Type of queued item;
- `S` - Number of slots;
- `C` - Auto complete flag;
- `A` - Default slot acquire attempts.

```c++
xtxn::fastest_mpmc_queue<payload_type, 256> queue {};
```
`payload_type` must have a default constructor.

### Constructor of `fastest_mpmc_queue`
```c++
fastest_mpmc_queue::fastest_mpmc_queue();
```

### Retrieving the queue state

#### Capacity
```c++
size_type fastest_mpmc_queue::capacity();
```
Returns current queue capacity in slots. Can only grow from the size of one block up to the maximum specified in the
template. When the maximum is reached and no free slots are available, the `producer_slot()` function will return an
invalid slot.

#### Free slots
```c++
size_type fastest_mpmc_queue::free_slots();
```
Returns number of free slots available to producers. This method is mostly useless under high producer and consumer
activity.

#### Emptiness
```c++
bool fastest_mpmc_queue::empty();
```
Returns whether the queue is empty. This method is mostly useless under high producer and consumer activity.
Should not be used as the sole basis for decision-making.

#### Producing
```c++
bool fastest_mpmc_queue::producing();
```
Returns whether producer activity is allowed. Recommended for organizing the producer loop.

#### Consuming
```c++
bool fastest_mpmc_queue::consuming();
```
Returns whether consumer activity is allowed. Recommended for organizing the consumer loop.

### Acquiring slot

#### Producer slot
```c++
producer_accessor fastest_mpmc_queue::producer_slot(int32_t slot_acquire_attempts = A);
```
Function to acquire a producer slot. Before use, the slot must be checked in a boolean context to ensure it's valid.
Any operations with an invalid slot result in undefined behavior.

#### Consumer slot
```c++
consumer_accessor fastest_mpmc_queue::consumer_slot(int32_t slot_acquire_attempts = A);
```
Function to acquire a consumer slot. Before use, the slot must be checked in a boolean context to ensure it's valid.
Any operations with an invalid slot result in undefined behavior.

### Stopping the queue loops

#### Stopping producing
```c++
void fastest_mpmc_queue::shutdown();
```
Stops producer loops. Essentially, it sets the flag returned by the `producing()` function.

#### Stopping any activities
```c++
void fastest_mpmc_queue::stop();
```
Stops both producing and consuming loops. Essentially, it sets the flag returned by the `producing()`
and `consuming()` functions.

### Slot accessor

Henceforth, the term `accessor` shall refer to either the `producer_accessor` or `consumer_accessor` type.

#### Validity check
```c++
bool accessor::operator bool();
```
Returns `true` if slot is valid, false otherwise. A valid slot allows obtaining a pointer or reference to its payload.
Performing these operations on an invalid slot results in undefined behavior.

#### Pointer to payload
```c++
T * producer_accessor::operator->();
const T * consumer_accessor::operator->();
```
Obtaining a pointer to the payload.

#### Reference to payload
```c++
T & producer_accessor::operator*();
const T & consumer_accessor::operator*();
```
Obtaining a reference to the payload.

#### Completion
```c++
void accessor::complete();
```
Mark the slot operations as completed. Calling this function is only required if the auto complete flag
(`C` parameter of template) is disabled.

## Examples

```c++
#include <xtxn/fastest_mpmc_queue.hpp>

xtxn::fastest_mpmc_queue<int, 256> queue {};

void queue_run() {
    std::jthread consumer1 { [] {
        while (queue.consuming()) {
            auto slot = queue.consumer_slot();
            if (slot) {
                // Get data from slot
                auto int_payload = *slot;
                // Do something...
            } else {
                std::this_thread::yield();
            }
        }
    } };

    std::jthread producer1 { [] {
        while (queue.producing()) {
            // Get and process some data
            int int_payload { 42 };
            while (queue.producing()) {
                auto slot = queue.producer_slot(xtxn::max_attempts);
                if (slot) {
                    // Store data to slot
                    *slot = int_payload;
                    break;
                } else {
                    std::this_thread::yield();
                }
            }
        }
    } };
}
```

```c++
#include <xtxn/fastest_mpmc_queue.hpp>

struct payload_type {
    std::string prop1 {};
    int prop2 {};
    void func1(int v) { prop2 = v; }
    auto func2() { return prop2; }
};

xtxn::fastest_mpmc_queue<payload_type, 256, false> queue {};

void queue_run() {
    std::jthread consumer1 { [] {
        while (queue.consuming()) {
            auto slot = queue.consumer_slot();
            if (slot) {
                // Get data from slot
                auto payload = *slot;
                auto prop1 = (*slot).prop1;
                auto prop2 = slot->prop2;
                auto prop3 = (*slot).func2();
                auto prop4 = slot->func2();
                // Do something...
                // Mark slot as processed
                slot.complete();
            } else {
                std::this_thread::yield();
            }
        }
    } };

    std::jthread producer1 { [] {
        while (queue.producing()) {
            // Get and process some data
            payload_type payload { .prop1 = "42", .prop2 = 42 };
            std::string prop1 { "42" };
            int prop2 { 42 };
            while (queue.producing()) {
                auto slot = queue.producer_slot(xtxn::max_attempts);
                if (slot) {
                    // Store data to slot
                    *slot = payload;
                    *slot = std::move(payload);
                    (*slot).prop1 = prop1;
                    slot->prop2 = prop2;
                    (*slot).func1(prop2);
                    auto prop3 = slot->func2();
                    // Mark slot as processed
                    slot.complete();
                    break;
                } else {
                    std::this_thread::yield();
                }
            }
        }
    } };
}
```

Several usage examples can be found in the `./src` and `./tests` directories.
