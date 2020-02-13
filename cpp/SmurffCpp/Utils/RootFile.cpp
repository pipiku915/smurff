#include "RootFile.h"

#include <iostream>
#include <fstream>

#include <highfive/H5File.hpp>

#include <Utils/Error.h>
#include <Utils/StringUtils.h>
#include <SmurffCpp/IO/GenericIO.h>
#include <SmurffCpp/StatusItem.h>



namespace h5 = HighFive;

namespace smurff {

const char* OPTIONS_TAG = "options";
const char* STEPS_TAG = "steps";
const char* STATUS_TAG = "status";
const char* LAST_CHECKPOINT_TAG = "last_checkpoint";
const char* CHECKPOINT_PREFIX = "checkpoint_";
const char* SAMPLE_PREFIX = "sample_";

RootFile::RootFile(std::string path, bool create)
   : m_path(path)
   , m_h5(path, create ? h5::File::Create : h5::File::ReadOnly)
{
}

std::string RootFile::getFullPath() const
{
   return m_path;
}

std::string RootFile::getPrefix() const
{
   return dirName(m_path);
}

std::string RootFile::getOptionsFileName() const
{
   return getPrefix() + "options.ini";
}
void RootFile::saveConfig(Config& config)
{
   std::string configPath = getOptionsFileName();
   config.save(configPath);
   m_h5.createAttribute<std::string>(OPTIONS_TAG, configPath);
}

std::string RootFile::restoreGetOptionsFileName() const
{
   std::string options_filename;
   m_h5.getAttribute(OPTIONS_TAG).read(options_filename);
   return options_filename;
}

void RootFile::restoreConfig(Config& config)
{
   //get options filename
   std::string optionsFileName = restoreGetOptionsFileName();

   //restore config
   bool success = config.restore(optionsFileName);
   THROWERROR_ASSERT_MSG(success, "Could not load ini file '" + optionsFileName + "'");
}

std::shared_ptr<StepFile> RootFile::createSampleStepFile(std::int32_t isample)
{
   return createStepFileInternal(isample, false);
}

std::shared_ptr<StepFile> RootFile::createCheckpointStepFile(std::int32_t isample)
{
   return createStepFileInternal(isample, true);
}

std::shared_ptr<StepFile> RootFile::createStepFileInternal(std::int32_t isample, bool checkpoint)
{
   return std::make_shared<StepFile>(m_h5, isample, checkpoint);
}

void RootFile::removeOldCheckpoints()
{
   std::string lastCheckpointItem;
   if (m_h5.hasAttribute(LAST_CHECKPOINT_TAG))
      m_h5.getAttribute(LAST_CHECKPOINT_TAG).read(lastCheckpointItem);

   std::vector<std::string> h5_objects = m_h5.listObjectNames();
   for (auto& name : h5_objects)
{
      if (startsWith(name, CHECKPOINT_PREFIX) && name != lastCheckpointItem)
         m_h5.unlink(name);
   }
}


std::shared_ptr<StepFile> RootFile::openLastCheckpoint() const
{
   if (m_h5.hasAttribute(LAST_CHECKPOINT_TAG))
   {
      std::string lastCheckpointItem;
      m_h5.getAttribute(LAST_CHECKPOINT_TAG).read(lastCheckpointItem);
      h5::Group group = m_h5.getGroup(lastCheckpointItem);
      return std::make_shared<StepFile>(m_h5, group);
   }

   return std::shared_ptr<StepFile>();
}

std::vector<std::shared_ptr<StepFile>> RootFile::openSampleStepFiles() const
{
   std::vector<std::string> h5_objects = m_h5.listObjectNames();
   std::vector<std::shared_ptr<StepFile>> samples;

   for (auto& name : h5_objects)
   {
      if (startsWith(name, SAMPLE_PREFIX))
      {
         h5::Group group = m_h5.getGroup(name);
         samples.push_back(std::make_shared<StepFile>(m_h5, group));
      }
   }

   return samples;
}

} // end namespace smurff
