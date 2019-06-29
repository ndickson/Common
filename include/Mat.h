#pragma once

#include "Types.h"
#include "Vec.h"

OUTER_NAMESPACE_START
COMMON_LIBRARY_NAMESPACE_START

template<typename SUBCLASS,typename T,size_t NROWS,size_t NCOLS=NROWS,bool ROW_MAJOR=true>
struct BaseMat {
protected:
	constexpr INLINE SUBCLASS& subclass() {
		return static_cast<SUBCLASS&>(*this);
	}
	constexpr INLINE const SUBCLASS& subclass() const {
		return static_cast<const SUBCLASS&>(*this);
	}
public:

	using BaseType = BaseMat<SUBCLASS,T,NROWS,NCOLS>;
	using ValueType = T;
	static constexpr size_t NumRows = NROWS;
	static constexpr size_t NumCols = NCOLS;
	static constexpr size_t NMAJOR = ROW_MAJOR ? NROWS : NCOLS;
	static constexpr size_t NMINOR = ROW_MAJOR ? NCOLS : NROWS;

protected:
	constexpr INLINE auto& operator[](size_t major) {
		return subclass()[major];
	}
	constexpr INLINE const auto& operator[](size_t major) const {
		return subclass()[major];
	}
	template<typename THAT_SUBCLASS,typename S,size_t MROWS,size_t MCOLS,bool THAT_ROW_MAJOR>
	friend struct BaseMat;
public:

	constexpr INLINE void negate() {
		for (size_t major = 0; major < NMAJOR; ++major) {
			subclass()[major] = -subclass()[major];
		}
	}
	constexpr INLINE const auto& transpose() const;
};

// Generic, fixed-dimension matrix class
template<typename T,size_t NROWS,size_t NCOLS,bool ROW_MAJOR>
struct Mat : public BaseMat<Mat<T,NROWS,NCOLS,ROW_MAJOR>,T,NROWS,NCOLS,ROW_MAJOR> {
	using BaseType = BaseMat<Mat<T,NROWS,NCOLS,ROW_MAJOR>,T,NROWS,NCOLS,ROW_MAJOR>;
	using BaseType::NMAJOR;
	using BaseType::NMINOR;
	using MajorType = Vec<T,NMINOR>;
	using ThisType = Mat<T,NROWS,NCOLS,ROW_MAJOR>;

	MajorType v[NMAJOR];

	INLINE Mat() = default;
	constexpr INLINE Mat(const ThisType& that) = default;
	constexpr INLINE Mat(ThisType&& that) = default;

	// Scaled identity matrix constructor
	constexpr INLINE Mat(const T that) : v{MajorType(T(0))} {
		constexpr size_t NSMALLER = (NMAJOR <= NMINOR) ? NMAJOR : NMINOR;
		if (that != T(0)) {
			for (size_t i = 0; i < NSMALLER; ++i) {
				v[i][i] = that;
			}
		}
	}

	// Transposing constructor
	// This allows Mat<T,N> tr(orig.transpose()); to work.
	INLINE Mat(const Mat<T,NROWS,NCOLS,!ROW_MAJOR>& that) {
		for (size_t major = 0; major < NMAJOR; ++major) {
			for (size_t minor = 0; minor < NMINOR; ++minor) {
				v[major][minor] = that[minor][major];
			}
		}
	}

	// Type conversion constructor
	template<typename S>
	explicit INLINE Mat(const Mat<S,NROWS,NCOLS,ROW_MAJOR>& that) {
		for (size_t major = 0; major < NMAJOR; ++major) {
			v[major] = Vec<T,NCOLS>(that[major]);
		}
	}

	// Type conversion transposing constructor
	template<typename S>
	explicit INLINE Mat(const Mat<S,NROWS,NCOLS,!ROW_MAJOR>& that) {
		for (size_t major = 0; major < NMAJOR; ++major) {
			for (size_t minor = 0; minor < NMINOR; ++minor) {
				v[major][minor] = T(that[minor][major]);
			}
		}
	}

	constexpr INLINE ThisType& operator=(const ThisType& that) = default;
	constexpr INLINE ThisType& operator=(ThisType&& that) = default;

	constexpr INLINE MajorType& operator[](size_t major) {
		// This static_assert needs to be in a function, because it refers to
		// ThisType, and the compiler doesn't let you reference the type that's
		// currently being compiled from class scope.
		static_assert(std::is_pod<ThisType>::value || !std::is_pod<T>::value, "Mat should be a POD type if T is.");

		return v[major];
	}
	constexpr INLINE const MajorType& operator[](size_t major) const {
		return v[major];
	}
};

// Mat == Mat
template<typename T0,size_t NROWS,size_t NCOLS,bool ROW_MAJOR0,typename T1,bool ROW_MAJOR1>
[[nodiscard]] constexpr INLINE bool operator==(const Mat<T0,NROWS,NCOLS,ROW_MAJOR0>& mat0, const Mat<T1,NROWS,NCOLS,ROW_MAJOR1>& mat1) {
	bool equal = true;
	// This could use branches to early-exit instead, but for small matrices,
	// it might be more efficient to do all of the comparisons,
	// instead of branching.
	constexpr size_t NMAJOR0 = ROW_MAJOR0 ? NROWS : NCOLS;
	if (ROW_MAJOR0 == ROW_MAJOR1) {
		for (size_t major = 0; major < NMAJOR0; ++major) {
			equal &= (mat0[major] == mat1[major]);
		}
	}
	else {
		constexpr size_t NMAJOR1 = ROW_MAJOR1 ? NROWS : NCOLS;
		for (size_t major0 = 0; major0 < NMAJOR0; ++major0) {
			for (size_t major1 = 0; major1 < NMAJOR1; ++major1) {
				equal &= (mat0[major0][major1] == mat1[major1][major0]);
			}
		}
	}
	return equal;
}
// Mat != Mat
template<typename T0,size_t NROWS,size_t NCOLS,bool ROW_MAJOR0,typename T1,bool ROW_MAJOR1>
[[nodiscard]] constexpr INLINE bool operator!=(const Mat<T0,NROWS,NCOLS,ROW_MAJOR0>& mat0, const Mat<T1,NROWS,NCOLS,ROW_MAJOR1>& mat1) {
	return !(*this == that);
}

// Mat.transpose()
template<typename SUBCLASS,typename T,size_t NROWS,size_t NCOLS,bool ROW_MAJOR>
[[nodiscard]] constexpr INLINE const auto& BaseMat<SUBCLASS,T,NROWS,NCOLS,ROW_MAJOR>::transpose() const {
	return *static_cast<const Mat<T,NROWS,NCOLS,!ROW_MAJOR>*>(this);
}

// +Mat (unary plus operator)
template<typename T,size_t NROWS,size_t NCOLS,bool ROW_MAJOR>
[[nodiscard]] constexpr INLINE const Mat<T,NROWS,NCOLS,ROW_MAJOR>& operator+(const Mat<T,NROWS,NCOLS,ROW_MAJOR>& mat) {
	return mat;
}
// -Mat (unary negation operator)
template<typename T,size_t NROWS,size_t NCOLS,bool ROW_MAJOR>
[[nodiscard]] constexpr INLINE const Mat<decltype(-T()),NROWS,NCOLS,ROW_MAJOR>& operator-(const Mat<T,NROWS,NCOLS,ROW_MAJOR>& mat) {
	using TS = decltype(-T());
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	Mat<TS,NROWS,NCOLS,ROW_MAJOR> out(TS(0));
	constexpr size_t NMAJOR = ROW_MAJOR ? NROWS : NCOLS;
	for (size_t major = 0; major < NMAJOR; ++major) {
		out[major] = -mat[major];
	}
	return out;
}

// Mat + Mat
template<typename T0,size_t NROWS,size_t NCOLS,bool ROW_MAJOR0,typename T1,bool ROW_MAJOR1>
[[nodiscard]] constexpr INLINE Mat<decltype(T0()+T1()),NROWS,NCOLS,ROW_MAJOR0> operator+(const Mat<T0,NROWS,NCOLS,ROW_MAJOR0>& mat0, const Mat<T1,NROWS,NCOLS,ROW_MAJOR1>& mat1) {
	using TS = decltype(T0()+T1());
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	Mat<TS,NROWS,NCOLS,ROW_MAJOR0> outMat(TS(0));
	constexpr size_t NMAJOR0 = ROW_MAJOR0 ? NROWS : NCOLS;
	if constexpr (ROW_MAJOR0 == ROW_MAJOR1) {
		for (size_t major = 0; major < NMAJOR0; ++major) {
			outMat[major] = (mat0[major] + mat1[major]);
		}
	}
	else {
		constexpr size_t NMAJOR1 = ROW_MAJOR1 ? NROWS : NCOLS;
		for (size_t major0 = 0; major0 < NMAJOR0; ++major0) {
			for (size_t major1 = 0; major1 < NMAJOR1; ++major1) {
				outMat[major0][major1] = (mat0[major0][major1] + mat1[major1][major0]);
			}
		}
	}
	return outMat;
}

// Mat - Mat
template<typename T0,size_t NROWS,size_t NCOLS,bool ROW_MAJOR0,typename T1,bool ROW_MAJOR1>
[[nodiscard]] constexpr INLINE Mat<decltype(T0()-T1()),NROWS,NCOLS,ROW_MAJOR0> operator-(const Mat<T0,NROWS,NCOLS,ROW_MAJOR0>& mat0, const Mat<T1,NROWS,NCOLS,ROW_MAJOR1>& mat1) {
	using TS = decltype(T0()-T1());
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	Mat<TS,NROWS,NCOLS,ROW_MAJOR0> outMat(TS(0));
	constexpr size_t NMAJOR0 = ROW_MAJOR0 ? NROWS : NCOLS;
	if constexpr (ROW_MAJOR0 == ROW_MAJOR1) {
		for (size_t major = 0; major < NMAJOR0; ++major) {
			outMat[major] = (mat0[major] - mat1[major]);
		}
	}
	else {
		constexpr size_t NMAJOR1 = ROW_MAJOR1 ? NROWS : NCOLS;
		for (size_t major0 = 0; major0 < NMAJOR0; ++major0) {
			for (size_t major1 = 0; major1 < NMAJOR1; ++major1) {
				outMat[major0][major1] = (mat0[major0][major1] - mat1[major1][major0]);
			}
		}
	}
	return outMat;
}

// Mat * scalar
template<typename T,size_t NROWS,size_t NCOLS,bool ROW_MAJOR,typename S>
[[nodiscard]] constexpr INLINE Mat<decltype(T()*S()),NROWS,NCOLS,ROW_MAJOR> operator*(const Mat<T,NROWS,NCOLS,ROW_MAJOR>& mat, const S& scalar) {
	using TS = decltype(T()*S());
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	Mat<TS,NROWS,NCOLS,ROW_MAJOR> outMat(TS(0));
	constexpr size_t NMAJOR = ROW_MAJOR ? NROWS : NCOLS;
	for (size_t major = 0; major < NMAJOR; ++major) {
		outMat[major] = (mat[major] * scalar);
	}
	return outMat;
}
// scalar * Mat
template<typename S,typename T,size_t NROWS,size_t NCOLS,bool ROW_MAJOR>
[[nodiscard]] constexpr INLINE Mat<decltype(S()*T()),NROWS,NCOLS,ROW_MAJOR> operator*(const S& scalar, const Mat<T,NROWS,NCOLS,ROW_MAJOR>& mat) {
	using TS = decltype(S()*T());
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	Mat<TS,NROWS,NCOLS,ROW_MAJOR> outMat(TS(0));
	constexpr size_t NMAJOR = ROW_MAJOR ? NROWS : NCOLS;
	for (size_t major = 0; major < NMAJOR; ++major) {
		outMat[major] = (scalar * mat[major]);
	}
	return outMat;
}

// Mat * Vec
template<typename T,size_t NROWS,size_t NCOLS,bool ROW_MAJOR,typename S>
[[nodiscard]] constexpr INLINE Vec<decltype(T()*S()),NROWS> operator*(const Mat<T,NROWS,NCOLS,ROW_MAJOR>& mat, const Vec<S,NCOLS>& vec) {
	using TS = decltype(T()*S());
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	Vec<TS,NROWS> outVec(TS(0));
	if constexpr (ROW_MAJOR) {
		for (size_t row = 0; row < NROWS; ++row) {
			TS v = mat[row][0] * vec[0];
			for (size_t col = 1; col < NCOLS; ++col) {
				v += (mat[row][col] * vec[col]);
			}
			outVec[row] = v;
		}
	}
	else {
		const S vecV = vec[0];
		for (size_t row = 0; row < NROWS; ++row) {
			outVec[row] = (mat[0][row] * vecV);
		}
		for (size_t col = 1; col < NCOLS; ++col) {
			const S vecV = vec[col];
			for (size_t row = 0; row < NROWS; ++row) {
				outVec[row] += (mat[col][row] * vecV);
			}
		}
	}
	return outVec;
}

// Vec * Mat
template<typename S,typename T,size_t NROWS,size_t NCOLS,bool ROW_MAJOR>
[[nodiscard]] constexpr INLINE Vec<decltype(S()*T()),NCOLS> operator*(const Vec<S,NROWS>& vec, const Mat<T,NROWS,NCOLS,ROW_MAJOR>& mat) {
	using TS = decltype(S()*T());
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	Vec<TS,NCOLS> outVec(TS(0));
	if constexpr (ROW_MAJOR) {
		const S vecV = vec[0];
		for (size_t col = 0; col < NCOLS; ++col) {
			outVec[col] = (mat[0][col] * vecV);
		}
		for (size_t row = 1; row < NROWS; ++row) {
			const S vecV = vec[row];
			for (size_t col = 0; col < NCOLS; ++col) {
				outVec[col] += (mat[row][col] * vecV);
			}
		}
	}
	else {
		for (size_t col = 0; col < NCOLS; ++col) {
			TS v = mat[col][0] * vec[0];
			for (size_t row = 1; row < NROWS; ++row) {
				v += (mat[col][row] * vec[row]);
			}
			outVec[col] = v;
		}
	}
	return outVec;
}

// Mat * Mat
template<typename T0,size_t NROWS,size_t NMID,size_t NCOLS,bool ROW_MAJOR0,typename T1,bool ROW_MAJOR1>
[[nodiscard]] constexpr INLINE Mat<decltype(T0()*T1()),NROWS,NCOLS,ROW_MAJOR0> operator*(const Mat<T0,NROWS,NMID,ROW_MAJOR0>& mat0, const Mat<T1,NMID,NCOLS,ROW_MAJOR1>& mat1) {
	using TS = decltype(T0()*T1());
	// NOTE: Initialization to zero is just so that the function can be constexpr.
	Mat<TS,NROWS,NCOLS,ROW_MAJOR0> outMat(TS(0));
	if constexpr (ROW_MAJOR0 && ROW_MAJOR1) {
		for (size_t row = 0; row < NROWS; ++row) {
			auto& outVec = outMat[row];
			auto& vec0 = mat0[row];
			for (size_t col = 0; col < NCOLS; ++col) {
				TS v = vec0[0] * mat1[0][col];
				for (size_t mid = 1; mid < NMID; ++mid) {
					v += vec0[mid] * mat1[mid][col];
				}
				outVec[col] = v;
			}
		}
	}
	else if constexpr (ROW_MAJOR0 && !ROW_MAJOR1) {
		for (size_t row = 0; row < NROWS; ++row) {
			auto& outVec = outMat[row];
			auto& vec0 = mat0[row];
			for (size_t col = 0; col < NCOLS; ++col) {
				auto& vec1 = mat1[col];
				outVec[col] = vec0.dot(vec1);
			}
		}
	}
	else if constexpr (!ROW_MAJOR0 && ROW_MAJOR1) {
		for (size_t col = 0; col < NCOLS; ++col) {
			auto& outVec = outMat[col];
			for (size_t row = 0; row < NROWS; ++row) {
				TS v = mat0[0][row] * mat1[0][col];
				for (size_t mid = 1; mid < NMID; ++mid) {
					v += mat0[mid][row] * mat1[mid][col];
				}
				outVec[row] = v;
			}
		}
	}
	else {
		for (size_t col = 0; col < NCOLS; ++col) {
			auto& outVec = outMat[col];
			auto& vec1 = mat1[col];
			for (size_t row = 0; row < NROWS; ++row) {
				TS v = mat0[0][row] * vec1[0];
				for (size_t mid = 1; mid < NMID; ++mid) {
					v += mat0[mid][row] * vec1[mid];
				}
				outVec[row] = v;
			}
		}
	}
	return outMat;
}

// Mat += Mat
template<typename T0,size_t NROWS,size_t NCOLS,bool ROW_MAJOR0,typename T1,bool ROW_MAJOR1>
[[nodiscard]] constexpr INLINE Mat<T0,NROWS,NCOLS,ROW_MAJOR0>& operator+=(Mat<T0,NROWS,NCOLS,ROW_MAJOR0>& mat0, const Mat<T1,NROWS,NCOLS,ROW_MAJOR1>& mat1) {
	constexpr size_t NMAJOR0 = ROW_MAJOR0 ? NROWS : NCOLS;
	if (ROW_MAJOR0 == ROW_MAJOR1) {
		for (size_t major = 0; major < NMAJOR0; ++major) {
			mat0[major] += mat1[major];
		}
	}
	else {
		constexpr size_t NMAJOR1 = ROW_MAJOR1 ? NROWS : NCOLS;
		for (size_t major0 = 0; major0 < NMAJOR0; ++major0) {
			for (size_t major1 = 0; major1 < NMAJOR1; ++major1) {
				mat0[major0][major1] += mat1[major1][major0];
			}
		}
	}
	return mat0;
}
// Mat -= Mat
template<typename T0,size_t NROWS,size_t NCOLS,bool ROW_MAJOR0,typename T1,bool ROW_MAJOR1>
[[nodiscard]] constexpr INLINE Mat<T0,NROWS,NCOLS,ROW_MAJOR0>& operator-=(Mat<T0,NROWS,NCOLS,ROW_MAJOR0>& mat0, const Mat<T1,NROWS,NCOLS,ROW_MAJOR1>& mat1) {
	constexpr size_t NMAJOR0 = ROW_MAJOR0 ? NROWS : NCOLS;
	if (ROW_MAJOR0 == ROW_MAJOR1) {
		for (size_t major = 0; major < NMAJOR0; ++major) {
			mat0[major] -= mat1[major];
		}
	}
	else {
		constexpr size_t NMAJOR1 = ROW_MAJOR1 ? NROWS : NCOLS;
		for (size_t major0 = 0; major0 < NMAJOR0; ++major0) {
			for (size_t major1 = 0; major1 < NMAJOR1; ++major1) {
				mat0[major0][major1] -= mat1[major1][major0];
			}
		}
	}
	return mat0;
}
// Mat *= scalar
template<typename T,size_t NROWS,size_t NCOLS,bool ROW_MAJOR,typename S>
[[nodiscard]] constexpr INLINE Mat<T,NROWS,NCOLS,ROW_MAJOR>& operator*=(const Mat<T,NROWS,NCOLS,ROW_MAJOR>& mat, const S& scalar) {
	constexpr size_t NMAJOR = ROW_MAJOR ? NROWS : NCOLS;
	for (size_t major = 0; major < NMAJOR; ++major) {
		mat[major] *= scalar;
	}
	return mat;
}

// Mat *= Mat
template<typename T0,size_t NROWS,size_t NCOLS,bool ROW_MAJOR0,typename T1,bool ROW_MAJOR1>
[[nodiscard]] constexpr INLINE Mat<T0,NROWS,NCOLS,ROW_MAJOR0>& operator*=(Mat<T0,NROWS,NCOLS,ROW_MAJOR0>& mat0, const Mat<T1,NCOLS,NCOLS,ROW_MAJOR1>& mat1) {
	mat0 = mat0*mat1;
	return mat0;
}

COMMON_LIBRARY_NAMESPACE_END
OUTER_NAMESPACE_END
