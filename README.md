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
template<std::default_initializable T, int32_t S = 0x10, int32_t L = 0x10'0000, growth_policy G = growth_policy::round>
class fast_mpmc_queue;
```
where
- T - Type of queued item;
- S - Number of slots per block;
- L - Maximum queue size (in slots);
- G - Growth policy (per call, round, or step).

```c++
xtxn::fast_mpmc_queue<payload_type> queue {};
```
`payload_type` must have a default constructor.

### Constructor
```c++
fast_mpmc_queue(int16_t producer_slot_acquire_attempts = 5);
```

### Retrieving the queue state
```c++
auto capacity();
```
Current queue capacity in slots. Can only grow from the size of one block up to the maximum specified in the template.
When the maximum is reached and no free slots are available, the `producer_slot()` function will return an invalid slot.

```c++
auto free_slots();
```
Number of free slots available to producers. This method is mostly useless under high producer and consumer activity.

```c++
bool empty();
```
Returns whether the queue is empty. This method is mostly useless under high producer and consumer activity.
Should not be used as the sole basis for decision-making.

```c++
bool producing();
```
Returns whether producer activity is allowed. Recommended for organizing the producer loop.

```c++
bool consuming();
```
Returns whether consumer activity is allowed. Recommended for organizing the consumer loop.

### Acquiring producer slot
```c++
producer_accessor producer_slot();
```
Function to acquire a producer slot. Before use, the slot must be checked in a boolean context to ensure it's valid.
Any operations with an invalid slot result in undefined behavior.

### Acquiring consumer slot
```c++
consumer_accessor consumer_slot();
```
Function to acquire a consumer slot. Before use, the slot must be checked in a boolean context to ensure it's valid.
Any operations with an invalid slot result in undefined behavior.

### Stopping the queue 
```c++
void shutdown();
```
Stops producer loops. Essentially, it sets the flag returned by the `producing()` function.

```c++
void stop();
```
Stops both producing and consuming loops. Essentially, it sets the flag
returned by the `producing()` and `consuming()` functions.

## Examples

```c++
xtxn::fast_mpmc_queue<int> queue {};

std::jthread consumer1 {
    [& queue] {
        while (queue.consuming()) {
            auto slot = queue.consumer_slot();
            if (slot) {
                // Do something
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
