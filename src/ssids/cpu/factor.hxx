/** \file
 *  \copyright 2016 The Science and Technology Facilities Council (STFC)
 *  \licence   BSD licence, see LICENCE file for details
 *  \author    Jonathan Hogg
 */
#pragma once

/* Standard headers */
#include <cmath>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <omp.h>
/* SPRAL headers */
#include "ssids/profile.hxx"
#include "ssids/cpu/cpu_iface.hxx"
#include "ssids/cpu/SymbolicNode.hxx"
#include "ssids/cpu/ThreadStats.hxx"
#include "ssids/cpu/Workspace.hxx"
#include "ssids/cpu/kernels/assemble.hxx"
#include "ssids/cpu/kernels/calc_ld.hxx"
#include "ssids/cpu/kernels/cholesky.hxx"
#include "ssids/cpu/kernels/ldlt_app.hxx"
#include "ssids/cpu/kernels/ldlt_tpp.hxx"
#include "ssids/cpu/kernels/wrappers.hxx"

//#include "ssids/cpu/kernels/verify.hxx" // FIXME: remove debug

namespace spral { namespace ssids { namespace cpu {

/* Factorize a node (indef) */
template <typename T, typename PoolAlloc>
void factor_node_indef(
      int ni, // FIXME: remove post debug
      SymbolicNode const& snode,
      NumericNode<T>* node,
      struct cpu_factor_options const& options,
      ThreadStats& stats,
      std::vector<Workspace>& work,
      PoolAlloc& pool_alloc
      ) {
   /* Extract useful information about node */
   int m = snode.nrow + node->ndelay_in;
   int n = snode.ncol + node->ndelay_in;
   size_t ldl = align_lda<T>(m);
   T *lcol = node->lcol;
   T *d = &node->lcol[ n*ldl ];
   int *perm = node->perm;
   T *contrib = node->contrib;

   /* Perform factorization */
   //Verify<T> verifier(m, n, perm, lcol, ldl);
   node->nelim = ldlt_app_factor(
         m, n, perm, lcol, ldl, d, 0.0, contrib, m-n, options, work, pool_alloc
         );
   //verifier.verify(node->nelim, perm, lcol, ldl, d);

   /* Finish factorization worth simplistic code */
   if(node->nelim < n) {
#ifdef PROFILE
      Profile::Task task_tpp("TA_LDLT_TPP");
#endif
      int nelim = node->nelim;
      stats.not_first_pass += n-nelim;
      T *ld = work[omp_get_thread_num()].get_ptr<T>(2*(m-nelim));
      node->nelim += ldlt_tpp_factor(
            m-nelim, n-nelim, &perm[nelim], &lcol[nelim*(ldl+1)], ldl,
            &d[2*nelim], ld, m-nelim, options.u, options.small, nelim,
            &lcol[nelim], ldl
            );
      if(m-n>0 && node->nelim>nelim) {
         int nelim2 = node->nelim - nelim;
         int ldld = align_lda<T>(m-n);
         T *ld = work[omp_get_thread_num()].get_ptr<T>(nelim2*ldld);
         calcLD<OP_N>(
               m-n, nelim2, &lcol[nelim*ldl+n], ldl, &d[2*nelim], ld, ldld
               );
         T rbeta = (nelim==0) ? 0.0 : 1.0;
         host_gemm<T>(OP_N, OP_T, m-n, m-n, nelim2,
               -1.0, &lcol[nelim*ldl+n], ldl, ld, ldld,
               rbeta, node->contrib, m-n);
      }
      stats.not_second_pass += n - node->nelim;
#ifdef profile
      task_tpp.done();
#endif
   }

#ifdef PROFILE
      Profile::setState("TA_MISC1");
#endif
   /* Record information */
   node->ndelay_out = n - node->nelim;
   stats.num_delay += node->ndelay_out;

   /* Mark as no contribution if we make no contribution */
   if(node->nelim==0 && !node->first_child && snode.contrib.size()==0) {
      // FIXME: Actually loop over children and check one exists with contrib
      //        rather than current approach of just looking for children.
      typedef std::allocator_traits<PoolAlloc> PATraits;
      PATraits::deallocate(pool_alloc, node->contrib, (m-n)*(m-n));
      node->contrib = nullptr;
   } else if(node->nelim==0) {
      // FIXME: If we fix the above, we don't need this explict zeroing
      long contrib_size = m-n;
      memset(node->contrib, 0, contrib_size*contrib_size*sizeof(T));
   }
}
/* Factorize a node (posdef) */
template <typename T>
void factor_node_posdef(
      T beta,
      SymbolicNode const& snode,
      NumericNode<T>* node,
      struct cpu_factor_options const& options,
      ThreadStats& stats
      ) {
   /* Extract useful information about node */
   int m = snode.nrow;
   int n = snode.ncol;
   int ldl = align_lda<T>(m);
   T *lcol = node->lcol;
   T *contrib = node->contrib;

   /* Perform factorization */
   int flag;
   cholesky_factor(
         m, n, lcol, ldl, beta, contrib, m-n, options.cpu_task_block_size, &flag
         );
   #pragma omp taskwait
   if(flag!=-1) {
      node->nelim = flag+1;
      stats.flag = Flag::ERROR_NOT_POS_DEF;
      return;
   }
   node->nelim = n;

   /* Record information */
   node->ndelay_out = 0;
}
/* Factorize a node (wrapper) */
template <bool posdef, typename T, typename PoolAlloc>
void factor_node(
      int ni,
      SymbolicNode const& snode,
      NumericNode<T>* node,
      struct cpu_factor_options const& options,
      ThreadStats& stats,
      std::vector<Workspace>& work,
      PoolAlloc& pool_alloc,
      T beta=0.0 // FIXME: remove once smallleafsubtree is doing own thing
      ) {
   if(posdef) factor_node_posdef(beta, snode, node, options, stats);
   else       factor_node_indef(ni, snode, node, options, stats, work, pool_alloc);
}

}}} /* end of namespace spral::ssids::cpu */
