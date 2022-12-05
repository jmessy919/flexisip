/** Copyright (C) 2010-2022 Belledonne Communications SARL
 *  SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "tester.hh"
#include "utils/test-patterns/test.hh"

using namespace std;

namespace flexisip {
namespace tester {
namespace dependency_injection {

template <typename T>
class Shield;
template <typename T>
class Ref;

template <typename T>
class ShieldState {
	friend Shield<T>;

public:
	void incRef() {
		mRefs++;
	}
	bool decRef() {
		if (mRefs <= 1) {
			return true;
		}
		mRefs--;
		return false;
	}

private:
	ShieldState(T& value) : mValue(&value), mRefs(1) {
	}

	T* mValue;
	uint mRefs;
};

template <typename T>
class Shield {
	friend Ref<T>;

public:
	explicit Shield(T& value) : mState(*new ShieldState<T>(value)) {
	}
	~Shield() {
		if (mState.decRef()) {
			delete &mState;
			return;
		}
		mState.mValue = nullptr;
	}

private:
	ShieldState<T>& mState;
};

template <typename T>
class Ref {
public:
	Ref(ShieldState<T>& state) : mState(state) {
		mState.incRef();
	}
	~Ref() {
		if (mState.decRef()) {
			delete &mState;
		}
	}
	Ref(Shield<T>& shield) : Ref(shield.mState) {
	}
	Ref(Ref<T>& other) : Ref(other.mState) {
	}
	Ref(Ref<T>&& other) : mState(other.mState) {
	}

	T* operator*() {
		return mState.mValue;
	}

private:
	ShieldState<T>& mState;
};

template <typename T>
class Shielded {
public:
	template <typename... Args>
	explicit Shielded(Args... args) : mValue(forward<Args>(args)...), mShield(mValue) {
	}

	T* operator->() {
		return &mValue;
	}

private:
	T mValue;
	Shield<T> mShield;
};

template <typename T>
class Dependency {
public:
	explicit Dependency(T& ref) : mPtr(&ref) {
	}
	Dependency(const Dependency<T>& other) = delete;
	Dependency<T>& operator=(const Dependency<T>& other) = delete;
	Dependency(Dependency<T>&& other) : mPtr(other.mPtr) {
		other.mPtr = nullptr;
	}
	Dependency<T>& operator=(Dependency<T>&& other) = delete;

	constexpr T* operator->() noexcept {
		return mPtr;
	}
	constexpr const T* operator->() const noexcept {
		return mPtr;
	}

private:
	T* mPtr;
};

namespace example {

class Incrementer {
public:
	int count() {
		return mCount;
	}

	void increment() {
		mCount++;
	}

private:
	int mCount = 0;
};

class DoubleIncrementer {
public:
	DoubleIncrementer(Dependency<Incrementer>&& inc1, Dependency<Incrementer>&& inc2)
	    : mInc1(move(inc1)), mInc2(move(inc2)) {
	}

	int count() {
		return mInc1->count() + mInc2->count();
	}

	void increment() {
		mInc1->increment();
		mInc2->increment();
	}

private:
	Dependency<Incrementer> mInc1;
	Dependency<Incrementer> mInc2;
};

} // namespace example

void dependency_injection() {
	example::Incrementer incs[2];
	example::DoubleIncrementer doubleInc{Dependency<example::Incrementer>(incs[0]),
	                                     Dependency<example::Incrementer>(incs[1])};
	doubleInc.increment();
	BC_ASSERT_EQUAL(doubleInc.count(), 2, int, "%i");

	{ Shielded<string> sting{"dayum"}; }
}

auto _ = [] {
	static test_t tests[] = {
	    CLASSY_TEST(dependency_injection),
	};
	static test_suite_t suite{"DependencyInjection", NULL, NULL, NULL, NULL, sizeof(tests) / sizeof(tests[0]), tests};
	bc_tester_add_suite(&suite);
	return nullptr;
}();

} // namespace dependency_injection
} // namespace tester
} // namespace flexisip
