// sigcore.h
#ifndef SIGCORE_H
#define SIGCORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifndef TSTDBG
// #define TSTDBG
#endif

/** @brief Generic pointer type for objects */
typedef void *object;
/** @brief Unsigned integer type representing memory addresses or offsets */
typedef uintptr_t addr;
/** @brief Pointer to a null-terminated character string */
typedef char *string;
/** @brief Pointer to an iterator_s structure for traversing collecions */
typedef struct iterator_s *iterator;
/** @brief Pointer to a string_builder_s structure for efficient string construction. */
typedef struct string_builder_s *string_builder;
/** @brief Pointer to a list_s structure.	*/
typedef struct list_s *list;
/** @brief Pointer to a queue_s structure. */
typedef struct queue_s *queue;
/** @brief Pointer to an index_s structure. */
typedef struct slot_array_s *slot_array;
/** @brief Predicate function */
typedef int (*predicate)(object);

/** @brief Iterator type for colleciton iterators */
typedef enum
{
	LIST,
	STRB,
	SLOT
} ITER_TYPE;

/**
 * @brief Interface for memory management in sigcore.
 * @details Provides allocation and deallocation functions for all sigcore components.
 */
typedef struct IMemCheck
{
	void (*enable)(void);	   /**< Enables memory leak detection. */
	void (*disable)(void);	   /**< Disables memory leak detection and reports leaks. */
	int (*isEnabled)(void);	   /**< Returns 1 if leak detection is enabled, 0 if not. */
	int (*leakedBlocks)(void); /**< Returns the number of leaked memory blocks */
	long (*leakedBytes)(void); /**< Returns the number of leaked bytes */
	void (*reset)(void);	   /**< Resets the leak detection state. */
} IMemCheck;
typedef struct IMemory
{
	object (*alloc)(size_t); /**< Allocates memory of the specified size. */
	void (*free)(object);	 /**< Frees previously allocated memory. */
	int (*track)(object);	 /**< Tracks an allocated object for leak detection. */
} IMemory;
/**
 * @brief Interface for managing dynamic lists of objects.
 * @details Defines operations for creating, modifying, and querying a list of addr pointers,
 *          leveraging Memcheck for memory management and Iterator for traversal.
 *				Contents must be manually freed by user.
 */
typedef struct IList
{
	list (*new)(int);						  /**< Creates a new list with the given capacity. */
	void (*free)(list);						  /**< Destroys the list and frees its memory. */
	void (*add)(list, object);				  /**< Adds an object to the list, resizing if needed. */
	int (*indexOf)(list, object);			  /**< Returns the index of an object, or -1 if not found. */
	object (*getAt)(list, int);				  /**< Returns the object at a specific index. */
	int (*copyTo)(list, list, int, int);	  /**< Copies items from one list to another. */
	void (*remove)(list, object);			  /**< Removes the first occurrence of an object. */
	int (*count)(list);						  /**< Returns the number of items in the list. */
	int (*capacity)(list);					  /**< Returns the current capacity of the list. */
	void (*clear)(list);					  /**< Clears all items from the list. */
	iterator (*iterateRange)(list, int, int); /**< Iterate item range */
} IList;
/**
 * @brief Interface for managing dynamic indexes of objects.
 * @details Defines operations for creating, modifying, and querying an index of addr pointers,
 * 		   leveraging Mem for memory management and Iterator for traversal.
 */
typedef struct ISlotArray
{
	slot_array (*new)(int);						/**< Creates a new index with the given capacity. */
	void (*free)(slot_array);					/**< Destroys the index and frees its memory. */
	void (*add)(slot_array, object);			/**< Adds an object to the index, resizing if needed. */
	int (*tryGetAt)(slot_array, int, object *); /**< Returns the object at a specific index. */
	void (*remove)(slot_array, object);			/**< Removes the first occurrence of an object. */
	int (*count)(slot_array);					/**< Returns the number of items in the index. */
	int (*capacity)(slot_array);				/**< Returns the current capacity of the index. */
	void (*clear)(slot_array);					/**< Clears all items from the index. */
												//	int (*copyTo)(slot_array, list);				  /**< Copies items from one index to another. */
} ISlotArray;
/**
 * @brief Interface for managing a dynamic queue (FIFO) of objects.
 * @details Defines operations for creating, enqueuing, dequeuing, and clearing a queue of
 *				addr pointers, leveraging Mem for memory management.
 *				Contents must be manually freed by user.
 */
typedef struct IQueue
{
	queue (*new)(int);				/**< Creates a new queue with the given capacity. */
	void (*free)(queue);			/**< Frees the queue and its memory. */
	void (*enqueue)(queue, object); /**< Adds an object to the rear of the queue. */
	object (*dequeue)(queue);		/**< Removes and returns the object at the front of the queue. */
	object (*peek)(queue);			/**< Returns the front object without removing it. */
	int (*count)(queue);			/**< Returns the number of items in the queue. */
	int (*capacity)(queue);			/**< Returns the current capacity of the queue. */
	void (*clear)(queue);			/**< Clears all items from the queue. */
	int (*isEmpty)(queue);			/**< Returns 1 if the queue is empty, otherwise 0. */
	int (*isFull)(queue);			/**< Returns 1 if the queue is full, otherwise 0. */
} IQueue;
/**
 * @brief Interface for basic string manipulation in sigcore.
 * @details Provides a clean set of utility functions for common string operations,
 *          abstracting away low-level details. Designed for simplicity and efficiency.
 */
typedef struct IString
{
	size_t (*length)(string);		  /**< Returns the length of a string. */
	string (*copy)(string);			  /**< Creates a copy of a string. */
	string (*dupe)(const char *);	  /**< Duplicates a string. */
	string (*concat)(string, string); /**< Returns a concatenated string. */
	string (*format)(string, ...);	  /**< Returns a formatted string. */
	int (*compare)(string, string);	  /**< Compares two strings for equality. */
	void (*free)(string);			  /**< Frees the string allocation. */
} IString;
typedef struct IArray
{
	iterator (*getIterator)(object, ITER_TYPE);
} IArray;
/**
 * @brief Interface for iterating over collections.
 * @details Provides methods to traverse a sequence of addr values, used by List and other structures.
 */
typedef struct IIterator
{
	object (*next)(iterator);						/**< Gets the next object and advances pointer to next index. */
	int (*hasNext)(iterator);						/**< Determins if the underlying collection has a next index. */
	int (*findNext)(iterator, predicate, object *); /**< Finds the next object matching the predicate. */
	void (*free)(iterator);							/**< Frees the iterator structure. */
	void (*reset)(iterator);						/**< Resets the iterator to the start. */
} IIterator;

/** @brief Global instance of the memory management interface. */
extern const IMemCheck MemCheck;
extern const IMemory Memory;
/** @brief Global instance of the list management interface. */
extern const IList List;
/** @brief Global instance of the slot array management interface. */
extern const ISlotArray SlotArray;
/** @brief Global instance of the queue iterface. */
extern const IQueue Queue;
/** #brief Global instance of the string interface. */
extern const IString String;
/** @brief Global instance of the array interface */
extern const IArray Array;
/** @brief Global instance of the iterator interface. */
extern const IIterator Iterator;

#endif // SIGCORE_H
