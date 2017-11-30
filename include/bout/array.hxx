/*
 * Handle arrays of data
 * 
 * Provides an interface to create, iterate over and release
 * arrays of templated types.
 *
 * Each type and array size has an object store, so when
 * arrays are released they are put into a store. Rather
 * than allocating memory, objects are retrieved from the
 * store. This minimises new and delete operations.
 * 
 * 
 * Ben Dudson, University of York, 2015
 *
 *
 * Changelog
 * ---------
 * 
 * 2015-03-04  Ben Dudson <bd512@york.ac.uk>
 *     o Initial version
 *
 */


#ifndef __ARRAY_H__
#define __ARRAY_H__

#include <map>
#include <vector>
#include <memory>

#ifdef BOUT_ARRAY_WITH_VALARRAY
#include <valarray>
#endif
/*!
 * Data array type with automatic memory management
 *
 * This implements a container similar to std::vector
 * but with reference counting like a smart pointer
 * and custom memory management to minimise new and delete calls
 * 
 * This can be used as an alternative to static arrays
 * 
 * Array<dcomplex> vals(100); // 100 complex numbers
 * 
 * vals[10] = 1.0;  // ok
 * 
 * When an Array goes out of scope or is deleted,
 * the underlying memory (ArrayData) is put into
 * a map, rather than being freed. 
 * If the same size arrays are used repeatedly then this
 * avoids the need to use new and delete.
 *
 * This behaviour can be disabled by calling the static function useStore:
 *
 * Array<dcomplex>::useStore(false); // Disables memory store
 * 
 */
template<typename T>
class Array {
public:
  typedef T data_type;
    
  /*!
   * Create an empty array
   * 
   * Array a();
   * a.empty(); // True
   * 
   */
  Array() : ptr(nullptr) {}
  
  /*!
   * Create an array of given length
   */
  Array(int len) {
    ptr = get(len);
  }
  
  /*!
   * Destructor. Releases the underlying ArrayData
   */
  ~Array() {
    release(ptr);
  }
  
  /*!
   * Copy constructor
   */
  Array(const Array &other) {
    ptr = other.ptr; 
  }

  /*!
   * Assignment operator
   * After this both Arrays share the same ArrayData
   */
  Array& operator=(const Array &other) {
    // Release the old data
    release(ptr);
    
    // Add reference to new data
    ptr = other.ptr;
    
    return *this;
  }

  /*!
   * Move constructor
   */
  Array(Array&& other) {
    ptr = std::move(other.ptr);
  }

  /*! 
   * Move assignment
   */
  Array& operator=(Array &&other) {
    release(ptr);
    ptr = std::move(other.ptr);
    
    return *this;
  }

  /*!
   * Holds a static variable which controls whether
   * memory blocks (ArrayData) are put into a store
   * or new/deleted each time. 
   *
   * The variable is initialised to true on first use,
   * but can be set to false by passing "false" as input.
   * Once set to false it can't be changed back to true.
   */
  static bool useStore( bool keep_using = true ) {
    static bool value = true;
    if (keep_using) {
      return value; 
    }
    // Change to false
    value = false;
    return value;
  }
  
  /*!
   * Release data. After this the Array is empty and any data access
   * will be invalid
   */
  void clear() {
    release(ptr);
  }

  /*!
   * Delete all data from the store and disable the store
   * 
   * Note: After this is called the store cannot be re-enabled
   */
  static void cleanup() {
    // Clean the store, deleting data
    store(true);
    // Don't use the store anymore. This is so that array releases
    // after cleanup() get deleted rather than put into the store
    useStore(false);
  }

  /*!
   * Returns true if the Array is empty
   */
  bool empty() const {
    return !ptr;
  }

  /*!
   * Return size of the array. Zero if the array is empty.
   */
  int size() const {
    if(!ptr)
      return 0;
#ifdef BOUT_ARRAY_WITH_VALARRAY
    return ptr->size();
#else
    return ptr->len;
#endif    
  }
  
  /*!
   * Returns true if the data is unique to this Array.
   * 
   */
  bool unique() const {
    return ptr.use_count() == 1;
  }

  /*!
   * Ensures that this Array does not share data with another
   * This should be called before performing any write operations
   * on the data.
   */
  void ensureUnique() {
    if(!ptr || unique())
      return;

    // Get a new (unique) block of data
    dataPtrType p = get(size());

    //Make copy of the underlying data
#ifdef BOUT_ARRAY_WITH_VALARRAY    
    p->operator=((*ptr));
#else
    for(iterator it = begin(), ip = p->begin(); it != end(); ++it, ++ip)
      *ip = *it;
#endif    

    //Update the local pointer and release old
    //Can't just do ptr=p as need to try to add to store.
    release(ptr);
    ptr = std::move(p);
  }

  //////////////////////////////////////////////////////////
  // Iterators -- only for non-valarray use currently
#ifndef BOUT_ARRAY_WITH_VALARRAY //Valarray version could probably use std::begin(ptr.get()) and std::end(...)
  typedef T* iterator;

  iterator begin() { 
    return (ptr) ? ptr->data : nullptr;
  }

  iterator end() {
    return (ptr) ? ptr->data + ptr->len : nullptr;
  }

  // Const iterators
  typedef const T* const_iterator;
  
  const_iterator begin() const {
    return (ptr) ? ptr->data : nullptr;
  }

  const_iterator end() const {
    return (ptr) ? ptr->data + ptr->len : nullptr;
  }
#endif
  //////////////////////////////////////////////////////////
  // Element access

  /*!
   * Access a data element. This will fail if the Array is empty (so ptr is null),
   * or if ind is out of bounds. For efficiency no checking is performed,
   * so the user should perform checks.
   */
  T& operator[](int ind) {
#ifdef BOUT_ARRAY_WITH_VALARRAY
    return ptr->operator[](ind);
#else    
    return ptr->data[ind];
#endif    
  }
  const T& operator[](int ind) const {
#ifdef BOUT_ARRAY_WITH_VALARRAY
    return ptr->operator[](ind);
#else    
    return ptr->data[ind];
#endif    
  }

  /*!
   * Exchange contents with another Array of the same type.
   * Sizes of the arrays may differ.
   *
   * This is called by the template function swap(Array&, Array&)
   */
  void swap(Array<T> &other) {
    std::swap(ptr,other.ptr);
  }
  
private:

#ifndef BOUT_ARRAY_WITH_VALARRAY  
  /*!
   * ArrayData holds the actual data, and reference count
   * Handles the allocation and deletion of data
   */
  struct ArrayData {
    int len;    ///< Size of the array
    T *data;    ///< Array of data
    
    ArrayData(int size) : len(size) {
      data = new T[len];
    }
    ~ArrayData() {
      delete[] data;
    }
    iterator begin() {
      return data;
    }
    iterator end() {
      return data + len;
    }
  };
#endif

    //Type defs to help keep things brief -- which backing do we use
#ifdef BOUT_ARRAY_WITH_VALARRAY
  typedef std::valarray<T> dataBlock;
#else
  typedef ArrayData dataBlock;
#endif

  typedef std::shared_ptr<dataBlock>  dataPtrType;

  /*!
   * Pointer to the data container object owned by this Array. 
   * May be null
   */
  dataPtrType ptr;

  typedef std::map< int, std::vector<dataPtrType> > storeType;
  typedef std::vector< storeType > arenaType;

  /*!
   * This maps from array size (int) to vectors of pointers to ArrayData objects
   *
   * By putting the static store inside a function it is initialised on first use,
   * and doesn't need to be separately declared for each type T
   *
   * Inputs
   * ------
   *
   * @param[in] cleanup   If set to true, deletes all ArrayData and clears the store
   */
  static storeType& store(bool cleanup=false) {
#ifdef _OPENMP    
    static arenaType arena(omp_get_max_threads());
#else
    static arenaType arena(1);
#endif
    
    if (!cleanup) {
#ifdef _OPENMP 
      return arena[omp_get_thread_num()];
#else
      return arena[0];
#endif
    }

    // Clean by deleting all data -- possible that just stores.clear() is
    // sufficient rather than looping over each entry.
#pragma omp single
    {
      for (auto &stores : arena) {
	for (auto &p : stores) {
	  auto &v = p.second;
	  for (dataPtrType a : v) {
	    a = nullptr; //Could use a.reset() if clearer
	  }
	  v.clear();
	}
	stores.clear();
      }
      //Here we ensure there is exactly one empty map still
      //left in the arena as we have to return one such item
      arena.resize(1);
    }

    //Store should now be empty but we need to return something,
    //so return an empty storeType from the arena.
    return arena[0];
  }
  
  /*!
   * Returns a pointer to an ArrayData object with no
   * references. This is either from the store, or newly allocated
   */
  dataPtrType get(int len) {
    dataPtrType p;

    auto& st = store()[len];
    
    if (!st.empty()) {
      p = st.back();
      st.pop_back();
    } else {
      p = std::make_shared<dataBlock>(len);
    }

    return p;
  }
  
  /*!
   * Release an ArrayData object, reducing its reference count by one. 
   * If no more references, then put back into the store.
   * It's important to pass a reference to the pointer, otherwise we get
   * a copy of the shared_ptr, which therefore increases the use count
   * and doesn't allow us to free the pass pointer directly
   */
  void release(dataPtrType &d) {
    if (!d)
      return;
    
    // Reduce reference count, and if zero return to store
    if(d.use_count()==1) {
      if (useStore()) {
	// Put back into store
#ifdef BOUT_ARRAY_WITH_VALARRAY
	store()[d->size()].push_back(std::move(d));
#else	  
	store()[d->len   ].push_back(std::move(d));
#endif
	//Could return here but seems to slow things down a lot
      }
    }

    //Finish by setting pointer to nullptr if not putting on store
    d=nullptr;
  }
  
};

/*!
 * Create a copy of an Array, which does not share data
 */ 
template<typename T>
Array<T>& copy(const Array<T> &other) {
  Array<T> a(other);
  a.ensureUnique();
  return a;
}

/*!
 * Exchange contents of two Arrays of the same type.
 * Sizes of the arrays may differ.
 */
template<typename T>
void swap(Array<T> &a, Array<T> &b) {
  a.swap(b);
}

#endif // __ARRAY_H__

