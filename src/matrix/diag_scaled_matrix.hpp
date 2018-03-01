#ifndef _TBLIS_DIAG_SCALED_MATRIX_HPP_
#define _TBLIS_DIAG_SCALED_MATRIX_HPP_

#include "util/basic_types.h"

namespace tblis
{

template <typename T, int RowCol>
class diag_scaled_matrix
{
    public:
        enum { COL_SCALED, ROW_SCALED };

    protected:
        T* data_ = nullptr;
        std::array<len_type,2> len_ = {};
        std::array<stride_type,2> stride_ = {};
        T* diag_ = nullptr;
        stride_type diag_stride_ = 0;

    public:
        diag_scaled_matrix() {}

        diag_scaled_matrix(len_type m, len_type n, T* ptr, stride_type rs, stride_type cs, T* diag, stride_type inc)
        : data_(ptr), len_{m, n}, stride_{rs, cs}, diag_(diag), diag_stride_(inc) {}

        void shift(unsigned dim, len_type n)
        {
            TBLIS_ASSERT(dim < 2);

            data_ += n*stride_[dim];
            if (dim == RowCol) diag_ += n*diag_stride_;
        }

        T* data() const
        {
            return data_;
        }

        T* diag() const
        {
            return diag_;
        }

        len_type length(unsigned dim) const
        {
            TBLIS_ASSERT(dim < 2);
            return len_[dim];
        }

        len_type length(unsigned dim, len_type len)
        {
            TBLIS_ASSERT(dim < 2);
            std::swap(len, len_[dim]);
            return len;
        }

        const std::array<len_type, 2>& lengths() const
        {
            return len_;
        }

        stride_type stride(unsigned dim) const
        {
            TBLIS_ASSERT(dim < 2);
            return stride_[dim];
        }

        stride_type stride(unsigned dim, stride_type stride)
        {
            TBLIS_ASSERT(dim < 2);
            std::swap(stride, stride_[dim]);
            return stride;
        }

        const std::array<stride_type, 2>& strides() const
        {
            return stride_;
        }

        stride_type diag_stride() const
        {
            return diag_stride_;
        }

        stride_type diag_stride(stride_type stride)
        {
            std::swap(stride, diag_stride_);
            return stride;
        }

        unsigned dimension() const
        {
            return 2;
        }

        void transpose()
        {
            using std::swap;
            swap(len_[0], len_[1]);
            swap(stride_[0], len_[1]);
        }

        void pack(const communicator& comm, const config& cfg, bool trans, normal_matrix<T>& Ap) const
        {
            const len_type MR = (!trans ? cfg.gemm_mr.def<T>()
                                        : cfg.gemm_nr.def<T>());
            const len_type ME = (!trans ? cfg.gemm_mr.extent<T>()
                                        : cfg.gemm_nr.extent<T>());
            const len_type KR = cfg.gemm_kr.def<T>();

            const len_type m_a = length( trans);
            const len_type k_a = length(!trans);
            const stride_type rs_a = stride( trans);
            const stride_type cs_a = stride(!trans);
            const stride_type inc_d = diag_stride();

            comm.distribute_over_threads({m_a, MR}, {k_a, KR},
            [&](len_type m_first, len_type m_last, len_type k_first, len_type k_last)
            {
                const T*  p_a =    data() +  m_first       *rs_a + k_first* cs_a;
                const T*  p_d =    diag() +                        k_first*inc_d;
                      T* p_ap = Ap.data() + (m_first/MR)*ME* k_a + k_first*   ME;

                for (len_type off_m = m_first;off_m < m_last;off_m += MR)
                {
                    len_type m = std::min(MR, m_last-off_m);
                    len_type k = k_last-k_first;

                    if (!trans)
                        cfg.pack_nnd_mr_ukr.call<T>(m, k, p_a, rs_a, cs_a, p_d, inc_d, p_ap);
                    else
                        cfg.pack_nnd_nr_ukr.call<T>(m, k, p_a, rs_a, cs_a, p_d, inc_d, p_ap);

                    p_a += m*rs_a;
                    p_ap += ME*k_a;
                }
            });
        }
};

}

#endif