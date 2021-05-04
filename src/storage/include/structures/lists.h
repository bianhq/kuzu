#pragma once

#include "src/common/include/vector/node_vector.h"
#include "src/storage/include/structures/common.h"
#include "src/storage/include/structures/lists_aux_structures.h"

namespace graphflow {
namespace storage {

// BaseLists is the top-level structure that holds a set of lists {of adjacent edges or rel
// properties}.
class BaseLists : public BaseColumnOrLists {

public:
    void readValues(const nodeID_t& nodeID, const shared_ptr<ValueVector>& valueVector,
        uint64_t& listLen, const unique_ptr<ColumnOrListsHandle>& handle,
        uint32_t maxElementsToRead);

protected:
    BaseLists(const string& fname, const DataType& dataType, const size_t& elementSize,
        shared_ptr<ListHeaders> headers, BufferManager& bufferManager)
        : BaseColumnOrLists{fname, dataType, elementSize, bufferManager}, metadata{fname},
          headers{headers} {};

    void readFromLargeList(const nodeID_t& nodeID, const shared_ptr<ValueVector>& valueVector,
        uint64_t& listLen, const unique_ptr<ColumnOrListsHandle>& handle, uint32_t header,
        uint32_t maxElementsToRead);

    void readSmallList(const nodeID_t& nodeID, const shared_ptr<ValueVector>& valueVector,
        uint64_t& listLen, const unique_ptr<ColumnOrListsHandle>& handle, uint32_t header);

public:
    constexpr static uint16_t LISTS_CHUNK_SIZE = 512;

protected:
    ListsMetadata metadata;
    shared_ptr<ListHeaders> headers;
};

// Lists<D> is the implementation of BaseLists for Lists of a specific datatype D.
template<DataType D>
class Lists : public BaseLists {

public:
    Lists(const string& fname, shared_ptr<ListHeaders> headers, BufferManager& bufferManager)
        : BaseLists{fname, D, getDataTypeSize(D), headers, bufferManager} {};
};

// Lists<NODE> is the specialization of Lists<D> for lists of nodeIDs.
template<>
class Lists<NODE> : public BaseLists {

public:
    Lists(const string& fname, BufferManager& bufferManager,
        NodeIDCompressionScheme nodeIDCompressionScheme)
        : BaseLists{fname, NODE, nodeIDCompressionScheme.getNumTotalBytes(),
              make_shared<ListHeaders>(fname), bufferManager},
          nodeIDCompressionScheme{nodeIDCompressionScheme} {};

    NodeIDCompressionScheme& getNodeIDCompressionScheme() { return nodeIDCompressionScheme; }

    shared_ptr<ListHeaders> getHeaders() { return headers; };

private:
    NodeIDCompressionScheme nodeIDCompressionScheme;
};

// Lists<UNKNOWN> is the specialization of Lists<D> for Node's unstructured PropertyLists. Though is
// shares the identical representation as BaseLists, it is more aligned to Columns in terms of
// access. In particular, readValues(...) of Lists<UNKNOWN> is given a NodeVector as input, similar
// to readValues() in Columns. For each node in NodeVector, unstructured property list of that node
// is read and the reqired property alongwith its dataType is copied to a specialized UNKNOWN-typed
// ValueVector.
template<>
class Lists<UNSTRUCTURED> : public BaseLists {

public:
    Lists(const string& fname, BufferManager& bufferManager)
        : BaseLists{fname, UNSTRUCTURED, 1, make_shared<ListHeaders>(fname), bufferManager},
          overflowPagesFileHandle{fname + ".ovf"} {};

    // readValues is overloaded. Lists<UNKNOWN> is not supposed to use the one defined in BaseLists.
    void readValues(const shared_ptr<NodeIDVector>& nodeIDVector, uint32_t propertyKeyIdxToRead,
        const shared_ptr<ValueVector>& valueVector, const unique_ptr<ColumnOrListsHandle>& handle);

private:
    void readUnstrPropertyListOfNode(const nodeID_t& nodeID, uint32_t propertyKeyIdxToRead,
        const shared_ptr<ValueVector>& valueVector, uint64_t pos,
        const unique_ptr<ColumnOrListsHandle>& handle, uint32_t header);

    void readUnstrPropertyKeyIdxAndDatatype(uint8_t* propertyKeyDataTypeCache,
        uint64_t& physicalPageIdx, const uint32_t*& propertyKeyIdxPtr,
        DataType& propertyKeyDataType, const unique_ptr<ColumnOrListsHandle>& handle,
        PageCursor& pageCursor, uint64_t& listLen, LogicalToPhysicalPageIdxMapper& mapper);

    void readOrSkipUnstrPropertyValue(uint64_t& physicalPageIdx, DataType& propertyDataType,
        const unique_ptr<ColumnOrListsHandle>& handle, PageCursor& pageCursor, uint64_t& listLen,
        LogicalToPhysicalPageIdxMapper& mapper, const shared_ptr<ValueVector>& valueVector,
        uint64_t pos, bool toRead);

    void readStringsFromOverflowPages(const shared_ptr<ValueVector>& valueVector);

public:
    static constexpr uint8_t UNSTR_PROP_IDX_LEN = 4;
    static constexpr uint8_t UNSTR_PROP_DATATYPE_LEN = 1;
    static constexpr uint8_t UNSTR_PROP_HEADER_LEN = UNSTR_PROP_IDX_LEN + UNSTR_PROP_DATATYPE_LEN;

    FileHandle overflowPagesFileHandle;
};

typedef Lists<INT32> RelPropertyListsInt;
typedef Lists<DOUBLE> RelPropertyListsDouble;
typedef Lists<BOOL> RelPropertyListsBool;
typedef Lists<STRING> RelPropertyListsString;
typedef Lists<NODE> AdjLists;
typedef Lists<UNSTRUCTURED> UnstructuredPropertyLists;

} // namespace storage
} // namespace graphflow
