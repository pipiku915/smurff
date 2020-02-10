#include <iostream>

#include <SmurffCpp/Utils/StepFile.h>

#include <SmurffCpp/Model.h>
#include <SmurffCpp/result.h>
#include <SmurffCpp/Priors/ILatentPrior.h>

#include <Utils/Error.h>
#include <Utils/StringUtils.h>
#include <SmurffCpp/IO/GenericIO.h>
#include <SmurffCpp/IO/MatrixIO.h>

#define LATENTS_SEC_TAG "latents"
#define PRED_SEC_TAG "predictions"
#define LINK_MATRICES_SEC_TAG "link_matrices"

#define IS_CHECKPOINT_TAG "is_checkpoint"
#define NUMBER_TAG "number"
#define NUM_MODES_TAG "num_modes"
#define PRED_TAG "pred"
#define PRED_STATE_TAG "pred_state"
#define PRED_AVG_TAG "pred_avg"
#define PRED_VAR_TAG "pred_var"

#define RMSE_AVG_TAG "rmse_avg"
#define RMSE_1SAMPLE_TAG "rmse_1sample"
#define AUC_AVG_TAG "auc_avg"
#define AUC_1SAMPLE_TAG "auc_1sample"
#define SAMPLE_ITER_TAG "sample_iter"
#define BURNIN_ITER_TAG "burnin_iter"

namespace smurff {

StepFile::StepFile(std::int32_t isample, h5::Group group, bool checkpoint, bool final)
   : m_isample(isample), m_group(group), m_checkpoint(checkpoint), m_final(final)
{
}

//name methods
bool StepFile::hasModel(std::uint64_t index) const
{
   return hasDataSet(LATENTS_SEC_TAG, LATENTS_PREFIX + std::to_string(index));
}

std::shared_ptr<Matrix> StepFile::getModel(std::uint64_t index) const
{
   return getMatrix(LATENTS_SEC_TAG, LATENTS_PREFIX + std::to_string(index));
}

void StepFile::putModel(std::uint64_t index, const Matrix &M) const
{
   putMatrix(LATENTS_SEC_TAG, LATENTS_PREFIX + std::to_string(index), M);
}

bool StepFile::hasLinkMatrix(std::uint32_t mode) const
{
   return hasDataSet(LINK_MATRICES_SEC_TAG, LINK_MATRIX_PREFIX + std::to_string(mode));
}

std::shared_ptr<Matrix> StepFile::getLinkMatrix(std::uint32_t mode) const
{
   return getMatrix(LINK_MATRICES_SEC_TAG, LINK_MATRIX_PREFIX + std::to_string(mode));
}

bool StepFile::hasMu(std::uint64_t index) const
{
   return hasDataSet(LINK_MATRICES_SEC_TAG, MU_PREFIX + std::to_string(index));
}

std::shared_ptr<Matrix> StepFile::getMu(std::uint64_t index) const
{
   return getMatrix(LINK_MATRICES_SEC_TAG, MU_PREFIX + std::to_string(index));
}

bool StepFile::hasPred() const
{
   return hasDataSet(PRED_SEC_TAG, PRED_AVG_TAG) && 
          hasDataSet(PRED_SEC_TAG, PRED_VAR_TAG);
}

void StepFile::putPredState(double rmse_avg, double rmse_1sample, double auc_avg, double auc_1sample,
                            int sample_iter, int burnin_iter) 
{
   auto pred_group = m_group.getGroup(PRED_SEC_TAG);
   pred_group.createAttribute<double>(RMSE_AVG_TAG, rmse_avg);
   pred_group.createAttribute<double>(RMSE_1SAMPLE_TAG, rmse_1sample);
   pred_group.createAttribute<double>(AUC_AVG_TAG, auc_avg);
   pred_group.createAttribute<double>(AUC_1SAMPLE_TAG, auc_1sample);
   pred_group.createAttribute<int>(SAMPLE_ITER_TAG, sample_iter);
   pred_group.createAttribute<int>(BURNIN_ITER_TAG, burnin_iter);
}

void StepFile::getPredState(
   double &rmse_avg, double &rmse_1sample, double &auc_avg, double &auc_1sample, int &sample_iter, int &burnin_iter) const
{
   auto pred_group = m_group.getGroup(PRED_SEC_TAG);
   pred_group.getAttribute(RMSE_AVG_TAG).read(rmse_avg);
   pred_group.getAttribute(RMSE_1SAMPLE_TAG).read(rmse_1sample);
   pred_group.getAttribute(AUC_AVG_TAG).read(auc_avg);
   pred_group.getAttribute>(AUC_1SAMPLE_TAG).read(auc_1sample);
   pred_group.getAttribute(SAMPLE_ITER_TAG).read(sample_iter);
   pred_group.getAttribute(BURNIN_ITER_TAG).read(burnin_iter);

}

void StepFile::putPredAvgVar(const SparseMatrix &avg, const SparseMatrix &var)
{
   putSparseMatrix(PRED_SEC_TAG, PRED_AVG_TAG, avg);
   putSparseMatrix(PRED_SEC_TAG, PRED_VAR_TAG, var);
}


std::shared_ptr<Matrix> StepFile::getPredAvg() const
{
   return getMatrix(PRED_SEC_TAG, PRED_AVG_TAG);
}

std::shared_ptr<Matrix> StepFile::getPredVar() const
{
   return getMatrix(PRED_SEC_TAG, PRED_VAR_TAG);
}

//save methods

void StepFile::save(
         std::shared_ptr<const Model> model,
         std::shared_ptr<const Result> pred,
   const std::vector<std::shared_ptr<ILatentPrior> >& priors
   ) const
{
   m_group.createAttribute<bool>(IS_CHECKPOINT_TAG, m_checkpoint);
   m_group.createAttribute<int>(NUMBER_TAG, m_isample);

   model->save(shared_from_this(), saveAggr);
   pred->save(shared_from_this());
   for (auto &p : priors) p->save(shared_from_this());
}

//restore methods

void StepFile::restoreModel(std::shared_ptr<Model> model, int skip_mode) const
{
   model->restore(shared_from_this(), skip_mode);

   int nmodes = model->nmodes();
   for(int i=0; i<nmodes; ++i)
   {
       std::shared_ptr<Matrix> mu, beta;
       if hasDataset(LINK_MATRICES_SEC_TAG, LINK_MATRIX_PREFIX + std::to_string(i))
         beta = matrix_io::eigen::read_matrix( LINK_MATRICES_SEC_TAG, LINK_MATRIX_PREFIX + std::to_string(i))); 

       if hasDataset(LINK_MATRICES_SEC_TAG, MU_PREFIX + std::to_string(i))
         mu = matrix_io::eigen::read_matrix( LINK_MATRICES_SEC_TAG, MU_PREFIX + std::to_string(i))); 

       model->setLinkMatrix(i, beta, mu);
   }
}

//-- used in PredictSession
std::shared_ptr<Model> StepFile::restoreModel(int skip_mode) const
{
    auto model = std::make_shared<Model>();
    restoreModel(model, skip_mode);
    return model;
}

void StepFile::restorePred(std::shared_ptr<Result> m_pred) const
{
   m_pred->restore(shared_from_this());
}

void StepFile::restorePriors(std::vector<std::shared_ptr<ILatentPrior> >& priors) const
{
   for (auto &p : priors) p->restore(shared_from_this());
}

void StepFile::restore(std::shared_ptr<Model> model, std::shared_ptr<Result> pred, std::vector<std::shared_ptr<ILatentPrior> >& priors) const
{
   restoreModel(model);
   restorePred(pred);
   restorePriors(priors);
}

//getters

std::int32_t StepFile::getIsample() const
{
   return m_isample;
}

bool StepFile::isCheckpoint() const
{
   return m_checkpoint;
}

//ini methods

bool StepFile::hasDataSet(const std::string& section, const std::string& tag) const
{
   if (!m_group.exist(section)) return false;
   auto section_group = m_group.getGroup(section);
   return (section_group.exist(tag));
}

std::shared_ptr<Matrix> StepFile::getMatrix(const std::string& section, const std::string& tag) const
{
   auto dataset = m_group.getGroup(section).getDataSet(tag);
   std::vector<size_t> dims = dataset.getDimensions();
   Matrix data(dims[0], dims[1]);
   dataset.read(data.data());

   if (data.IsVectorAtCompileTime || data.IsRowMajor)
      return std::shared_ptr<Matrix>(data);

   // convert to ColMajor if needed (HDF5 always stores row-major)
   std::make_shared<Matrix>(Eigen::Map<Eigen::Matrix<
            typename Matrix::Scalar,
            Matrix::RowsAtCompileTime,
            Matrix::ColsAtCompileTime,
            Matrix::ColsAtCompileTime==1?Eigen::ColMajor:Eigen::RowMajor,
            Matrix::MaxRowsAtCompileTime,
            Matrix::MaxColsAtCompileTime>>(data.data(), dims[0], dims[1]));
}

std::shared_ptr<SparseMatrix> StepFile::getSparseMatrix(const std::string& section, const std::string& tag) const
{
   auto sparse_group = m_group.getGroup(section).getGroup(tag); 

   std::string format;
   sparse_group.getAttribute("h5sparse_format").read(format);
   THROWERROR_ASSERT(( SparseMatrix::IsRowMajor && format == "csr") || \
                     (!SparseMatrix::IsRowMajor && format == "csc"));
   
   std::vector<Eigen::Index> shape(2);
   sparse_group.getAttribute("h5sparse_shape").read(shape);
   SparseMatrix X(shape.at(0), shape.at(1));
   X.makeCompressed();

   auto data = sparse_group.getDataSet("data");
   THROWERROR_ASSERT(data.getDataType() == h5::AtomicType<SparseMatrix::value_type>());
   X.resizeNonZeros(data.getElementCount());
   data.read(X.valuePtr());

   auto indptr = sparse_group.getDataSet("indptr");
   THROWERROR_ASSERT(indptr.getDataType() == h5::AtomicType<SparseMatrix::Index>());
   indptr.read(X.outerIndexPtr());

   auto indices = sparse_group.getDataSet("indices");
   THROWERROR_ASSERT(indices.getDataType() == h5::AtomicType<SparseMatrix::Index>());
   indices.read(X.innerIndexPtr());

   return std::shared_ptr<SparseMatrix>(X);
}

void StepFile::putMatrix(const std::string& section, const std::string& tag, const Matrix &M) const
{
   h5::Group group = m_group.getGroup(section);
   h5::DataSet dataset = group.createDataSet<Matrix::Scalar>(DataSpace::From(M)));

   Eigen::Ref<
        const Eigen::Matrix<
            Matrix::Scalar,
            Matrix::RowsAtCompileTime,
            Matrix::ColsAtCompileTime,
            Matrix::ColsAtCompileTime==1?Eigen::ColMajor:Eigen::RowMajor,
            Matrix::MaxRowsAtCompileTime,
            Matrix::MaxColsAtCompileTime>,
        0,
        Eigen::InnerStride<1>> row_major(M);

    dataset.write(row_major.data());
}

void StepFile::putSparseMatrix(const std::string& section, const std::string& tag, const SparseMatrix &X) const
{
   h5::Group sparse_group = m_group.getGroup(section).createGroup(tag);

   sparse_group.createAttribute<std::string>("h5sparse_format", (SparseMatrix::IsRowMajor ? "csr" : "csc"));
   std::vector<Eigen::Index> shape{X.innerSize(), X.outerSize()};
   sparse_group.createAttribute<Eigen::Index>("h5sparse_shape", h5::DataSpace::From(shape)).write(shape);

   auto data = sparse_group.createDataSet<SparseMatrix::value_type>("data", h5::DataSpace(X.nonZeros()));
   data.write(X.valuePtr());

   auto indptr = sparse_group.createDataSet<SparseMatrix::Index>("indptr", h5::DataSpace(X.outerSize() + 1));
   indptr.write(X.outerIndexPtr());

   auto indices = sparse_group.createDataSet<SparseMatrix::Index>("indices", h5::DataSpace(X.nonZeros()));
   indices.write(X.innerIndexPtr());
}


/*
void StepFile::appendToStepFile(std::string section) const
{
   if (m_group.hasGroup(section)) return;
   m_group.createGroup(section);
}
*/

} // end namespace smurff
