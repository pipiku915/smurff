#pragma once

#include <vector>
#include <iostream>
#include <memory>
#include <cstdint>
#include <algorithm>

#include <SmurffCpp/Types.h>
#include <SmurffCpp/Utils/PVec.hpp>
#include <SmurffCpp/Configs/NoiseConfig.h>

namespace smurff
{
   class Data;
   class IDataWriter;
   class IDataCreator;
   class ConfigFile;

   class DataConfig
   {
   private:
      NoiseConfig m_noiseConfig;

   protected:
      bool m_isDense;
      bool m_isBinary;
      bool m_isScarce;
      bool m_isMatrix;

      std::vector<std::uint64_t> m_dims;
      std::uint64_t m_nnz;

   private:
      PVec<>      m_pos;
      std::string m_filename;

      Matrix       m_dense_matrix_data;
      SparseMatrix m_sparse_matrix_data;
      Tensor       m_dense_tensor_data;
      SparseTensor m_sparse_tensor_data;

   public:
      virtual ~DataConfig();

      DataConfig() {}

   protected:
      DataConfig(bool isDense, bool isBinary, bool isScarce,
                 std::vector<std::uint64_t> m_dims, std::uint64_t nnz, 
                 const NoiseConfig& noiseConfig, PVec<> pos = {});

   public:
      void setData(const       Matrix &m);
      void setData(const SparseMatrix &m);
      void setData(const       Tensor &m);
      void setData(const SparseTensor &m);

      const Matrix       &getDenseMatrixData()  const;
      const SparseMatrix &getSparseMatrixData() const;
      const SparseTensor &getSparseTensorData() const;

      const NoiseConfig& getNoiseConfig() const;
      void setNoiseConfig(const NoiseConfig& value);
     
      bool isMatrix() const;
      bool isDense() const;
      bool isBinary() const;
      bool isScarce() const;

      std::uint64_t getNModes() const;
      std::uint64_t getNNZ() const;

      const std::vector<std::uint64_t>& getDims() const;
      std::uint64_t getNRow() const { return getDims().at(0); }
      std::uint64_t getNCol() const { return getDims().at(1); }

      void setFilename(const std::string& f);
      const std::string &getFilename() const;

      void setPos(const PVec<>& p);
      void setPos(const std::vector<int> &p) { setPos(PVec<>(p)); }
      bool hasPos() const;
      const PVec<> &getPos() const;

   public:
      virtual std::ostream& info(std::ostream& os) const;
      virtual std::string info() const;

      void save(ConfigFile& writer, const std::string& section_name) const;
      bool restore(const ConfigFile& reader, const std::string& sec_name);

   public:
      virtual std::shared_ptr<Data> create(std::shared_ptr<IDataCreator> creator) const;

   public:
      virtual void write(std::shared_ptr<IDataWriter> writer) const;

   public:
      void check() const;
   };
}
