from libc.stdint cimport *
from libcpp.memory cimport shared_ptr, make_shared
from libcpp.vector cimport vector

from Config cimport Config, stringToPriorType, stringToModelInitType
from ISession cimport ISession
from NoiseConfig cimport *
from MatrixConfig cimport MatrixConfig
from TensorConfig cimport TensorConfig
from SessionFactory cimport SessionFactory

cimport numpy as np
import  numpy as np
import  scipy as sp

def remove_nan(Y):
    if not np.any(np.isnan(Y.data)):
        return Y
    idx = np.where(np.isnan(Y.data) == False)[0]
    return sp.sparse.coo_matrix( (Y.data[idx], (Y.row[idx], Y.col[idx])), shape = Y.shape )

cdef MatrixConfig* prepare_sparse(X, isScarce):
    if type(X) not in [sp.sparse.coo.coo_matrix, sp.sparse.csr.csr_matrix, sp.sparse.csc.csc_matrix]:
        raise ValueError("Matrix must be either coo, csr or csc (from scipy.sparse)")
    X = X.tocoo(copy = False)
    X = remove_nan(X)
    cdef np.ndarray[uint32_t] irows = X.row.astype(np.uint32, copy=False)
    cdef np.ndarray[uint32_t] icols = X.col.astype(np.uint32, copy=False)
    cdef np.ndarray[double] vals = X.data.astype(np.double, copy=False)

    # Get begin and end pointers from numpy arrays
    cdef uint32_t* irows_begin = &irows[0]
    cdef uint32_t* irows_end = irows_begin + irows.shape[0]
    cdef uint32_t* icols_begin = &icols[0]
    cdef uint32_t* icols_end = icols_begin + icols.shape[0]
    cdef double* vals_begin = &vals[0]
    cdef double* vals_end = vals_begin + vals.shape[0]

    # Create vectors from pointers
    cdef vector[uint32_t]* rows_vector_ptr = new vector[uint32_t]()
    rows_vector_ptr.assign(irows_begin, irows_end)
    cdef vector[uint32_t]* cols_vector_ptr = new vector[uint32_t]()
    cols_vector_ptr.assign(icols_begin, icols_end)
    cdef vector[double]* vals_vector_ptr = new vector[double]()
    vals_vector_ptr.assign(vals_begin, vals_end)

    cdef shared_ptr[vector[uint32_t]] rows_vector_shared_ptr = shared_ptr[vector[uint32_t]](rows_vector_ptr)
    cdef shared_ptr[vector[uint32_t]] cols_vector_shared_ptr = shared_ptr[vector[uint32_t]](cols_vector_ptr)
    cdef shared_ptr[vector[double]] vals_vector_shared_ptr = shared_ptr[vector[double]](vals_vector_ptr)

    cdef MatrixConfig* matrix_config_ptr = new MatrixConfig(<uint64_t>(X.shape[0]), <uint64_t>(X.shape[1]), rows_vector_shared_ptr, cols_vector_shared_ptr, vals_vector_shared_ptr, NoiseConfig(), isScarce)
    return matrix_config_ptr

class ResultItem:
    def __init__(self, coords, val, pred_1sample, pred_avg, var, stds):
        self.coords = coords
        self.val = val
        self.pred_1sample = pred_1sample
        self.pred_avg = pred_avg
        self.var = var
        self.stds = stds

    def __str__(self):
        return "{}: {} | 1sample: {} | avg: {} | var: {} | stds: {}".format(self.coords, self.val, self.pred_1sample, self.pred_avg, self.var, self.stds)

    def __repr__(self):
        return str(self)

def smurff(Y,
           Ytest,
           row_features   = [],
           col_features   = [],
           row_prior      = None,
           col_prior      = None,
           lambda_beta    = 5.0,
           num_latent     = 10,
           precision      = 1.0,
           sn_init        = None,
           sn_max         = None,
           burnin         = 50,
           nsamples       = 400,
           tol            = 1e-6,
           direct         = True,
           seed           = None,
           threshold      = None,
           verbose        = True,
           quite          = False,
           init_model     = None,
           save_prefix    = None,
           save_suffix    = None,
           save_freq      = None,
           restore_prefix = None,
           restore_suffix = None,
           csv_status     = None):

    # Create and initialize smurff-cpp Config instance
    cdef Config config
    cdef NoiseConfig nc

    if precision:
        nc.setNoiseType(fixed)
        nc.precision = precision

    if sn_init and sn_max:
        nc.setNoiseType(adaptive)
        nc.sn_init = sn_init
        nc.sn_max = sn_max

    config.setTrain(shared_ptr[TensorConfig](prepare_sparse(Y, True)))
    config.getTrain().get().setNoiseConfig(nc)

    config.setTest(shared_ptr[TensorConfig](prepare_sparse(Ytest, True)))
    config.getTest().get().setNoiseConfig(nc)

    cdef shared_ptr[MatrixConfig] rf_matrix_config
    for rf in row_features:
        rf_matrix_config.reset(prepare_sparse(rf, False))
        rf_matrix_config.get().setNoiseConfig(nc)
        config.getRowFeatures().push_back(rf_matrix_config)

    cdef shared_ptr[MatrixConfig] cf_matrix_config
    for cf in col_features:
        cf_matrix_config.reset(prepare_sparse(cf, False))
        cf_matrix_config.get().setNoiseConfig(nc)
        config.getColFeatures().push_back(cf_matrix_config)

    if row_prior:
        config.setRowPriorType(stringToPriorType(row_prior))

    if col_prior:
        config.setColPriorType(stringToPriorType(col_prior))

    config.setLambdaBeta(lambda_beta)
    config.setNumLatent(num_latent)
    config.setBurnin(burnin)
    config.setNSamples(nsamples)
    config.setTol(tol)
    config.setDirect(direct)

    if seed:
        config.setRandomSeedSet(True)
        config.setRandomSeed(seed)

    if threshold:
        config.setThreshold(threshold)
        config.setClassify(True)

    config.setVerbose(verbose)
    if quite:
        config.setVerbose(False)

    if init_model:
        config.setModelInitType(stringToModelInitType(init_model))

    if save_prefix:
        config.setSavePrefix(save_prefix)

    if save_suffix:
        config.setSaveSuffix(save_suffix)

    if save_freq:
        config.setSaveFreq(save_freq)

    if restore_prefix:
        config.setRestorePrefix(restore_prefix)

    if restore_suffix:
        config.setRestoreSuffix(restore_suffix)

    if csv_status:
        config.setCsvStatus(csv_status)

    # Create and run session
    cdef shared_ptr[ISession] session = SessionFactory.create_py_session(config)
    session.get().init()
    for i in range(config.getNSamples() + config.getBurnin()):
        session.get().step()

    # Create Python list of ResultItem from C++ vector of ResultItem
    cpp_result_items = session.get().getResult()
    py_result_items = []
    for i in range(cpp_result_items.size()):
        cpp_result_item_ptr = &(cpp_result_items[i])
        py_result_item_coords = []
        for coord_index in range(cpp_result_item_ptr.coords.size()):
            coord = cpp_result_item_ptr.coords[coord_index]
            py_result_item_coords.append(coord)
        py_result_item = ResultItem( tuple(py_result_item_coords)
                                   , cpp_result_item_ptr.val
                                   , cpp_result_item_ptr.pred_1sample
                                   , cpp_result_item_ptr.pred_avg
                                   , cpp_result_item_ptr.var
                                   , cpp_result_item_ptr.stds
                                   )
        py_result_items.append(py_result_item)

    return py_result_items