#include <memory>

#include <SmurffCpp/Types.h>
#include <SmurffCpp/Types.h>

#include <Utils/counters.h>
#include <SmurffCpp/Utils/OutputFile.h>
#include <SmurffCpp/Utils/MatrixUtils.h>
#include <SmurffCpp/result.h>
#include <SmurffCpp/ResultItem.h>

#include <SmurffCpp/Model.h>

#include <SmurffCpp/Predict/PredictSession.h>

namespace smurff
{

PredictSession::PredictSession(const std::string &model_file)
    : m_model_rootfile(OutputFile(model_file))
    , m_pred_rootfile()
    , m_has_config(false)
    , m_num_latent(-1)
    , m_dims(PVec<>(0))
    , m_is_init(false)
{
    m_stepfiles = m_model_rootfile.openSampleSteps();
}

PredictSession::PredictSession(const Config &config)
    : PredictSession(config.getRootName())
{
    m_config = config;
    m_has_config = true;
}

void PredictSession::run()
{
    THROWERROR_ASSERT(m_has_config);


    if (m_config.getTest().hasData())
    {
        init();
        while (step())
            ;

        return;
    }
    else
    {
        std::pair<int, const DataConfig &> side_info =
            (m_config.getRowFeatures().hasData()) ?
            std::make_pair(0, m_config.getRowFeatures()) :
            std::make_pair(1, m_config.getColFeatures()) ;

        THROWERROR_ASSERT_MSG(!side_info.second.hasData(), "Need either test, row features or col features");

        if (side_info.second.isDense())
        {
            const auto &dense_matrix = side_info.second.getDenseMatrixData();
            predict(side_info.first, dense_matrix, m_config.getSaveFreq());
        }
        else
        {
            const auto &sparse_matrix = side_info.second.getSparseMatrixData();
            predict(side_info.first, sparse_matrix, m_config.getSaveFreq());
        }
    }
}

void PredictSession::init()
{
    THROWERROR_ASSERT(m_has_config);
    THROWERROR_ASSERT(m_config.getTest().hasData());
    m_result = Result(m_config.getTest(), m_config.getNSamples());

    m_pos = m_stepfiles.rbegin();
    m_iter = 0;
    m_is_init = true;

    THROWERROR_ASSERT_MSG(m_config.getOutputFilename() != m_model_rootfile.getFullPath(),
                          "Cannot have same output file for model and predictions - both have " + m_config.getOutputFilename());

    if (m_config.getSaveFreq())
    {
        // create root file
        m_pred_rootfile = std::make_unique<OutputFile>(m_config.getOutputFilename(), true);
    }

    if (m_config.getVerbose())
        info(std::cout, "");
}

bool PredictSession::step()
{
    THROWERROR_ASSERT(m_has_config);
    THROWERROR_ASSERT(m_is_init);
    THROWERROR_ASSERT(m_pos != m_stepfiles.rend());

    double start = tick();
    Model model;
    restoreModel(model, *m_pos);
    m_result.update(model, false);
    double stop = tick();
    m_iter++;
    m_secs_per_iter = stop - start;
    m_secs_total += m_secs_per_iter;

    if (m_config.getVerbose())
        std::cout << getStatus().asString() << std::endl;

    if (m_config.getSaveFreq() > 0 && (m_iter % m_config.getSaveFreq()) == 0)
        save();

    auto next_pos = m_pos;
    next_pos++;
    bool last_iter = next_pos == m_stepfiles.rend();

    //save last iter
    if (last_iter && m_config.getSaveFreq() == -1)
        save();

    m_pos++;
    return !last_iter;
}

void PredictSession::save()
{
    //save this iteration
    SaveState saveState = m_pred_rootfile->createSampleStep(m_iter);

    if (m_config.getVerbose())
    {
        std::cout << "-- Saving predictions into '" << m_pred_rootfile->getFullPath() << "'." << std::endl;
    }

    m_result.save(saveState);
}

StatusItem PredictSession::getStatus() const
{
    StatusItem ret;
    ret.phase = "Predict";
    ret.iter = m_pos->getIsample();
    ret.phase_iter = m_stepfiles.size();

    ret.train_rmse = NAN;

    ret.rmse_avg = m_result.rmse_avg;
    ret.rmse_1sample = m_result.rmse_1sample;

    ret.auc_avg = m_result.auc_avg;
    ret.auc_1sample = m_result.auc_1sample;

    ret.elapsed_iter = m_secs_per_iter;
    ret.elapsed_total = m_secs_total;

    return ret;
}

const Result &PredictSession::getResult() const
{
    return m_result;
}

std::ostream &PredictSession::info(std::ostream &os, std::string indent) const
{
    os << indent << "PredictSession {\n";
    os << indent << "  Model {\n";
    os << indent << "    model root-file: " << m_model_rootfile.getFullPath() << "\n";
    os << indent << "    num-samples: " << getNumSteps() << "\n";
    os << indent << "    num-latent: " << getNumLatent() << "\n";
    os << indent << "    dimensions: " << getModelDims() << "\n";
    os << indent << "  }\n";
    os << indent << "  Predictions {\n";
    m_result.info(os, indent + "    ");
    if (m_config.getSaveFreq() > 0)
    {
        os << indent << "    Save predictions: every " << m_config.getSaveFreq() << " iteration\n";
        os << indent << "    Output file: " << getOutputFilename() << "\n";
    }
    else if (m_config.getSaveFreq() < 0)
    {
        os << indent << "    Save predictions after last iteration\n";
        os << indent << "    Output file: " << getOutputFilename() << "\n";
    }
    else
    {
        os << indent << "    Don't save predictions\n";
    }
    os << indent << "  }" << std::endl;
    os << indent << "}\n";
    return os;
}

void PredictSession::restoreModel(Model &model, const SaveState &sf, int skip_mode)
{
    model.restore(sf, skip_mode);

    if (m_num_latent <= 0)
    {
        m_num_latent = model.nlatent();
        m_dims = model.getDims();
    }
    else
    {
        THROWERROR_ASSERT(m_num_latent == model.nlatent());
        THROWERROR_ASSERT(m_dims == model.getDims());
    }

    THROWERROR_ASSERT(m_num_latent > 0);
}

void PredictSession::restoreModel(Model &model, int i, int skip_mode)
{
    restoreModel(model, m_stepfiles.at(i), skip_mode);
}

// predict one element
ResultItem PredictSession::predict(PVec<> pos, const SaveState &sf)
{
    ResultItem ret{pos};
    predict(ret, sf);
    return ret;
}

// predict one element
void PredictSession::predict(ResultItem &res, const SaveState &sf)
{
    Model model;
    model.restore(sf);
    auto pred = model.predict(res.coords);
    res.update(pred);
}

// predict one element
void PredictSession::predict(ResultItem &res)
{
    auto stepfiles = m_model_rootfile.openSampleSteps();

    for (const auto &sf : stepfiles)
        predict(res, sf);
}

ResultItem PredictSession::predict(PVec<> pos)
{
    ResultItem ret{pos};
    predict(ret);
    return ret;
}

// predict all elements in Ytest
std::shared_ptr<Result> PredictSession::predict(const DataConfig &Y)
{
    auto res = std::make_shared<Result>(Y);

    for (const auto s : m_stepfiles)
    {
        Model model;
        restoreModel(model, s);
        res->update(model, false);
    }

    return res;
}

} // end namespace smurff
