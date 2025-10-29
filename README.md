# Fast Lock-Free Multi-Producer/Multi-Consumer Queue

This is an implementation of a lock-free multi-producer/multi-consumer queue in C++. The queue consists of slots
allocated in blocks as needed. Communication between producers and consumers occurs through writing and reading
information to/from these slots. This minimizes the number of allocations, deallocations, and memory fragmentation
while maintaining consistent performance over time.

Message order is not guaranteed, but the queue strives to preserve it.

## API

```c++
#include <xtxn/fast_mpmc_queue.hpp>
```
Include the header file containing the template class declaration:

```c++
template<
    std::default_initializable T,
    bool C = true,
    int32_t S = queue::default_block_size,
    int32_t L = queue::default_capacity_limit,
    int32_t A = queue::default_attempts,
    queue::growth_policy G = queue::growth_policy::round
>
class fast_mpmc_queue;
```
where
- T - Type of queued item;
- C - Auto complete flag;
- S - Number of slots per block;
- L - Maximum queue size (in slots);
- A - Default producer slot acquire attempts;
- G - Growth policy (per call, round, or step).

```c++
xtxn::fast_mpmc_queue<payload_type> queue {};
```
`payload_type` must have a default constructor.

### Constructor of `fast_mpmc_queue`
```c++
fast_mpmc_queue();
```

### Retrieving the queue state

#### Capacity
```c++
int32_t fast_mpmc_queue::capacity();
```
Returns current queue capacity in slots. Can only grow from the size of one block up to the maximum specified in the
template. When the maximum is reached and no free slots are available, the `producer_slot()` function will return an
invalid slot.

#### Free slots
```c++
int32_t fast_mpmc_queue::free_slots();
```
Returns number of free slots available to producers. This method is mostly useless under high producer and consumer
activity.

#### Emptiness
```c++
bool fast_mpmc_queue::empty();
```
Returns whether the queue is empty. This method is mostly useless under high producer and consumer activity.
Should not be used as the sole basis for decision-making.

#### Producing
```c++
bool fast_mpmc_queue::producing();
```
Returns whether producer activity is allowed. Recommended for organizing the producer loop.

#### Consuming
```c++
bool fast_mpmc_queue::consuming();
```
Returns whether consumer activity is allowed. Recommended for organizing the consumer loop.

### Acquiring slot

#### Producer slot
```c++
producer_accessor fast_mpmc_queue::producer_slot(int32_t slot_acquire_attempts = 0);
```
Function to acquire a producer slot. Before use, the slot must be checked in a boolean context to ensure it's valid.
Any operations with an invalid slot result in undefined behavior.

#### Consumer slot
```c++
consumer_accessor fast_mpmc_queue::consumer_slot();
```
Function to acquire a consumer slot. Before use, the slot must be checked in a boolean context to ensure it's valid.
Any operations with an invalid slot result in undefined behavior.

### Stopping the queue loops

#### Stopping producing
```c++
void fast_mpmc_queue::shutdown();
```
Stops producer loops. Essentially, it sets the flag returned by the `producing()` function.

#### Stopping any activities
```c++
void fast_mpmc_queue::stop();
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
T * accessor::operator->();
```
Obtaining a pointer to the payload.

#### Reference to payload
```c++
T & accessor::operator*();
```
Obtaining a reference to the payload.

#### Completion
```c++
void accessor::complete();
```
Mark the slot operations as completed. Calling this function is only required if the auto_complete parameter is
disabled.

## Examples

```c++
xtxn::fast_mpmc_queue<int> queue {};

std::jthread consumer1 {
    [& queue] {
        while (queue.consuming()) {
            auto slot = queue.consumer_slot();
            if (slot) {
                // Do something
                slot.complete();
            } else {
                std::this_thread::yield();
            }
        }
    }
};

std::jthread producer1 {
    [& queue] {
        while (queue.producing()) {
            // Get and process some data
            while (queue.producing()) {
                auto slot = queue.producer_slot(xtxn::max_attempts);
                if (slot) {
                    // Enqueue data
                    slot.complete();
                    break;
                } else {
                    std::this_thread::yield();
                }
            }
        }
    }
};
```

Several usage examples can be found in the `./src` directory. The repository also contains classical implementations
of MPSC and MPMC queues that were used for performance comparison.
