#pragma once

namespace WAVM {
	// A smart pointer that uses an intrusive reference counter.
	// Needs Pointee to define memory functions to manipulate the reference count:
	//   void addRef();
	//   void removeRef();
	template<typename Pointee> struct IntrusiveSharedPtr
	{
		// Constructors/destructor
		IntrusiveSharedPtr() : value(nullptr) {}
		IntrusiveSharedPtr(Pointee* inValue)
		{
			value = inValue;
			if(value) { value->addRef(); }
		}
		IntrusiveSharedPtr(const IntrusiveSharedPtr<Pointee>& inCopy)
		{
			value = inCopy.value;
			if(value) { value->addRef(); }
		}
		IntrusiveSharedPtr(IntrusiveSharedPtr<Pointee>&& inMove)
		{
			value = inMove.value;
			inMove.value = nullptr;
		}

		~IntrusiveSharedPtr()
		{
			if(value) { value->removeRef(); }
		}

		// Assignment operators
		void operator=(Pointee* inValue)
		{
			auto oldValue = value;
			value = inValue;
			if(value) { value->addRef(); }
			if(oldValue) { oldValue->removeRef(); }
		}
		void operator=(const IntrusiveSharedPtr<Pointee>& inCopy)
		{
			auto oldValue = value;
			value = inCopy.value;
			if(value) { value->addRef(); }
			if(oldValue) { oldValue->removeRef(); }
		}
		void operator=(IntrusiveSharedPtr<Pointee>&& inMove)
		{
			auto oldValue = value;
			value = inMove.value;
			inMove.value = nullptr;
			if(oldValue) { oldValue->removeRef(); }
		}

		// Accessors
		operator Pointee*() const { return value; }
		Pointee& operator*() const { return *value; }
		Pointee* operator->() const { return value; }

	private:
		Pointee* value;
	};
}
