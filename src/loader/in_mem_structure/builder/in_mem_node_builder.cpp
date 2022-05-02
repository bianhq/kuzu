#include "src/loader/include/in_mem_structure/builder/in_mem_node_builder.h"

#include "spdlog/spdlog.h"

#include "src/loader/include/in_mem_structure/in_mem_pages.h"
#include "src/loader/include/loader_task.h"
#include "src/storage/include/storage_structure/lists/unstructured_property_lists.h"

namespace graphflow {
namespace loader {

InMemNodeBuilder::InMemNodeBuilder(label_t nodeLabel, const NodeFileDescription& fileDescription,
    string outputDirectory, TaskScheduler& taskScheduler, Catalog& catalog,
    LoaderProgressBar* progressBar)
    : InMemStructuresBuilder{nodeLabel, fileDescription.labelName, fileDescription.filePath,
          move(outputDirectory), fileDescription.csvReaderConfig, taskScheduler, catalog,
          progressBar},
      IDType{fileDescription.IDType} {}

unique_ptr<NodeIDMap> InMemNodeBuilder::load() {
    logger->info("Loading node {} with label {}.", labelName, label);
    addLabelToCatalogAndCountLines();
    nodeIDMap = make_unique<NodeIDMap>(catalog.getNumNodes(label));
    initializeColumnsAndList();
    // Construct columns and list.
    populateColumnsAndCountUnstrPropertyListSizes();
    if (unstrPropertyLists) {
        calcUnstrListsHeadersAndMetadata();
        populateUnstrPropertyLists();
    }
    saveToFile();
    logger->info("Done loading node {} with label {}.", labelName, label);
    return move(nodeIDMap);
}

void InMemNodeBuilder::addLabelToCatalogAndCountLines() {
    // Parse csv header and calculate num blocks.
    vector<PropertyDefinition> colDefinitions;
    numBlocks = parseCSVHeaderAndCalcNumBlocks(inputFilePath, colDefinitions);
    // Count lines for each block in csv file, and parse unstructured properties.
    auto blockUnstrPropertyNames =
        countLinesPerBlockAndParseUnstrPropertyNames(colDefinitions.size());
    // Add label to the catalog.
    unordered_set<string> unstructuredPropertyNameSet;
    for (auto& unstrPropertiesInBlock : blockUnstrPropertyNames) {
        for (auto& propertyName : unstrPropertiesInBlock) {
            unstructuredPropertyNameSet.insert(propertyName);
        }
    }
    // Ensure that unstructured properties always have the same order in different platforms.
    vector<string> unstructuredPropertyNames{
        unstructuredPropertyNameSet.begin(), unstructuredPropertyNameSet.end()};
    sort(unstructuredPropertyNames.begin(), unstructuredPropertyNames.end());
    auto numNodes = 0;
    numLinesPerBlock[0]--; // Decrement the header line.
    for (auto blockId = 0u; blockId < numBlocks; blockId++) {
        numNodes += numLinesPerBlock[blockId];
    }
    catalog.addNodeLabel(
        labelName, IDType, move(colDefinitions), unstructuredPropertyNames, numNodes);
}

void InMemNodeBuilder::initializeColumnsAndList() {
    logger->info("Initializing in memory structured columns and unstructured list.");
    auto structuredProperties = catalog.getStructuredNodeProperties(label);
    structuredColumns.resize(structuredProperties.size());
    for (auto& property : structuredProperties) {
        auto fName =
            StorageUtils::getNodePropertyColumnFName(outputDirectory, label, property.name);
        structuredColumns[property.id] = InMemColumnFactory::getInMemPropertyColumn(
            fName, property.dataType, catalog.getNumNodes(label));
    }
    if (!catalog.getUnstructuredNodeProperties(label).empty()) {
        unstrPropertyLists = make_unique<InMemUnstructuredLists>(
            StorageUtils::getNodeUnstrPropertyListsFName(outputDirectory, label),
            catalog.getNumNodes(label));
    }
    logger->info("Done initializing in memory structured columns and unstructured list.");
}

vector<unordered_set<string>> InMemNodeBuilder::countLinesPerBlockAndParseUnstrPropertyNames(
    uint64_t numStructuredProperties) {
    logger->info("Counting number of lines and read unstructured property names in each block.");
    vector<unordered_set<string>> blockUnstrPropertyNames(numBlocks);
    numLinesPerBlock.resize(numBlocks);
    progressBar->addAndStartNewJob("Counting lines in the file for node: " + labelName, numBlocks);
    for (uint64_t blockId = 0; blockId < numBlocks; blockId++) {
        taskScheduler.scheduleTask(LoaderTaskFactory::createLoaderTask(
            countLinesAndParseUnstrPropertyNamesInBlockTask, inputFilePath, numStructuredProperties,
            &blockUnstrPropertyNames[blockId], blockId, this));
    }
    taskScheduler.waitAllTasksToCompleteOrError();
    logger->info(
        "Done counting number of lines and read unstructured property names  in each block.");
    return blockUnstrPropertyNames;
}

void InMemNodeBuilder::populateColumnsAndCountUnstrPropertyListSizes() {
    logger->debug("Populating structured properties and Counting unstructured properties.");
    node_offset_t offsetStart = 0;
    progressBar->addAndStartNewJob("Populating property columns for node: " + labelName, numBlocks);
    for (auto blockIdx = 0u; blockIdx < numBlocks; blockIdx++) {
        taskScheduler.scheduleTask(LoaderTaskFactory::createLoaderTask(
            populateColumnsAndCountUnstrPropertyListSizesTask, blockIdx, offsetStart, this));
        offsetStart += numLinesPerBlock[blockIdx];
    }
    taskScheduler.waitAllTasksToCompleteOrError();
    logger->debug("Done populating structured properties and counting unstructured properties.");
}

void InMemNodeBuilder::populateColumnsAndCountUnstrPropertyListSizesTask(
    uint64_t blockId, uint64_t offsetStart, InMemNodeBuilder* builder) {
    builder->logger->trace("Start: path={0} blkIdx={1}", builder->inputFilePath, blockId);
    auto structuredProperties = builder->catalog.getStructuredNodeProperties(builder->label);
    vector<PageByteCursor> overflowCursors(structuredProperties.size());
    CSVReader reader(builder->inputFilePath, builder->csvReaderConfig, blockId);
    if (0 == blockId) {
        if (reader.hasNextLine()) {
            reader.skipLine(); // skip header line.
        }
    }
    auto bufferOffset = 0u;
    while (reader.hasNextLine()) {
        putPropsOfLineIntoColumns(builder->structuredColumns, builder->nodeIDMap.get(),
            structuredProperties, overflowCursors, reader, offsetStart + bufferOffset);
        if (builder->unstrPropertyLists) {
            calcLengthOfUnstrPropertyLists(
                reader, offsetStart + bufferOffset, builder->unstrPropertyLists.get());
        }
        bufferOffset++;
    }
    builder->progressBar->incrementTaskFinished();
    builder->logger->trace("End: path={0} blkIdx={1}", builder->inputFilePath, blockId);
}

void InMemNodeBuilder::calcLengthOfUnstrPropertyLists(
    CSVReader& reader, node_offset_t nodeOffset, InMemUnstructuredLists* unstrPropertyLists) {
    assert(unstrPropertyLists);
    while (reader.hasNextToken()) {
        auto unstrPropertyString = reader.getString();
        auto startPos = strchr(unstrPropertyString, ':') + 1;
        *strchr(startPos, ':') = 0;
        InMemListsUtils::incrementListSize(*unstrPropertyLists->getListSizes(), nodeOffset,
            UnstructuredPropertyLists::UNSTR_PROP_HEADER_LEN +
                Types::getDataTypeSize(Types::dataTypeFromString(string(startPos))));
    }
}

void InMemNodeBuilder::countLinesAndParseUnstrPropertyNamesInBlockTask(const string& fName,
    uint32_t numStructuredProperties, unordered_set<string>* unstrPropertyNameSet, uint32_t blockId,
    InMemNodeBuilder* builder) {
    builder->logger->trace("Start: path=`{0}` blkIdx={1}", fName, blockId);
    CSVReader reader(fName, builder->csvReaderConfig, blockId);
    builder->numLinesPerBlock[blockId] = 0ull;
    while (reader.hasNextLine()) {
        builder->numLinesPerBlock[blockId]++;
        for (auto i = 0u; i < numStructuredProperties; ++i) {
            reader.hasNextToken();
        }
        while (reader.hasNextToken()) {
            auto unstrPropertyStr = reader.getString();
            auto unstrPropertyName =
                StringUtils::split(unstrPropertyStr, LoaderConfig::UNSTR_PROPERTY_SEPARATOR)[0];
            unstrPropertyNameSet->insert(unstrPropertyName);
        }
    }
    builder->logger->trace("End: path=`{0}` blkIdx={1}", fName, blockId);
    builder->progressBar->incrementTaskFinished();
}

void InMemNodeBuilder::calcUnstrListsHeadersAndMetadata() {
    if (unstrPropertyLists == nullptr) {
        return;
    }
    logger->debug("Initializing UnstructuredPropertyListHeaders.");
    progressBar->addAndStartNewJob("Calculating lists headers for node: " + labelName, 1);
    taskScheduler.scheduleTask(LoaderTaskFactory::createLoaderTask(calculateListHeadersTask,
        catalog.getNumNodes(label), 1, unstrPropertyLists->getListSizes(),
        unstrPropertyLists->getListHeaders(), logger, progressBar));
    logger->debug("Done initializing UnstructuredPropertyListHeaders.");
    taskScheduler.waitAllTasksToCompleteOrError();
    logger->debug("Initializing UnstructuredPropertyListsMetadata.");
    progressBar->addAndStartNewJob("Calculating lists metadata for node: " + labelName, 1);
    taskScheduler.scheduleTask(
        LoaderTaskFactory::createLoaderTask(calculateListsMetadataTask, catalog.getNumNodes(label),
            1, unstrPropertyLists->getListSizes(), unstrPropertyLists->getListHeaders(),
            unstrPropertyLists->getListsMetadata(), false /*hasNULLBytes*/, logger, progressBar));
    logger->debug("Done initializing UnstructuredPropertyListsMetadata.");
    taskScheduler.waitAllTasksToCompleteOrError();
}

void InMemNodeBuilder::populateUnstrPropertyLists() {
    if (unstrPropertyLists == nullptr) {
        return;
    }
    logger->debug("Populating Unstructured Property Lists.");
    node_offset_t offsetStart = 0;
    progressBar->addAndStartNewJob(
        "Populating unstructured property lists for node: " + labelName, numBlocks);
    unstrPropertyLists->init();
    for (auto blockIdx = 0u; blockIdx < numBlocks; blockIdx++) {
        taskScheduler.scheduleTask(LoaderTaskFactory::createLoaderTask(
            populateUnstrPropertyListsTask, offsetStart, blockIdx, this));
        offsetStart += numLinesPerBlock[blockIdx];
    }
    taskScheduler.waitAllTasksToCompleteOrError();
    logger->debug("Done populating Unstructured Property Lists.");
}

void InMemNodeBuilder::populateUnstrPropertyListsTask(
    uint64_t blockId, node_offset_t offsetStart, InMemNodeBuilder* builder) {
    builder->logger->trace("Start: path={0} blkIdx={1}", builder->inputFilePath, blockId);
    CSVReader reader(builder->inputFilePath, builder->csvReaderConfig, blockId);
    if (0 == blockId) {
        if (reader.hasNextLine()) {}
    }
    auto bufferOffset = 0u;
    PageByteCursor overflowPagesCursor;
    auto numStructuredProperties =
        builder->catalog.getStructuredNodeProperties(builder->label).size();
    auto unstrPropertiesNameToIdMap =
        builder->catalog.getUnstrPropertiesNameToIdMap(builder->label);
    assert(!unstrPropertiesNameToIdMap.empty());
    while (reader.hasNextLine()) {
        for (auto i = 0u; i < numStructuredProperties; ++i) {
            reader.hasNextToken();
        }
        putUnstrPropsOfALineToLists(reader, offsetStart + bufferOffset, overflowPagesCursor,
            unstrPropertiesNameToIdMap,
            reinterpret_cast<InMemUnstructuredLists*>(builder->unstrPropertyLists.get()));
        bufferOffset++;
    }
    builder->progressBar->incrementTaskFinished();
    builder->logger->trace("End: path={0} blkIdx={1}", builder->inputFilePath, blockId);
}

void InMemNodeBuilder::putPropsOfLineIntoColumns(vector<unique_ptr<InMemColumn>>& structuredColumns,
    NodeIDMap* nodeIDMap, const vector<PropertyDefinition>& structuredProperties,
    vector<PageByteCursor>& overflowCursors, CSVReader& reader, uint64_t nodeOffset) {
    for (auto columnIdx = 0u; columnIdx < structuredColumns.size(); columnIdx++) {
        reader.hasNextToken();
        auto column = structuredColumns[columnIdx].get();
        switch (column->getDataType().typeID) {
        case INT64: {
            if (!reader.skipTokenIfNull()) {
                auto int64Val = reader.getInt64();
                column->setElement(nodeOffset, reinterpret_cast<uint8_t*>(&int64Val));
                if (structuredProperties[columnIdx].isPrimaryKey) {
                    nodeIDMap->set(to_string(int64Val).c_str(), nodeOffset);
                }
            }
        } break;
        case DOUBLE: {
            if (!reader.skipTokenIfNull()) {
                auto doubleVal = reader.getDouble();
                column->setElement(nodeOffset, reinterpret_cast<uint8_t*>(&doubleVal));
            }
        } break;
        case BOOL: {
            if (!reader.skipTokenIfNull()) {
                auto boolVal = reader.getBoolean();
                column->setElement(nodeOffset, reinterpret_cast<uint8_t*>(&boolVal));
            }
        } break;
        case DATE: {
            if (!reader.skipTokenIfNull()) {
                auto dateVal = reader.getDate();
                column->setElement(nodeOffset, reinterpret_cast<uint8_t*>(&dateVal));
            }
        } break;
        case TIMESTAMP: {
            if (!reader.skipTokenIfNull()) {
                auto timestampVal = reader.getTimestamp();
                column->setElement(nodeOffset, reinterpret_cast<uint8_t*>(&timestampVal));
            }
        } break;
        case INTERVAL: {
            if (!reader.skipTokenIfNull()) {
                auto intervalVal = reader.getInterval();
                column->setElement(nodeOffset, reinterpret_cast<uint8_t*>(&intervalVal));
            }
        } break;
        case STRING: {
            if (!reader.skipTokenIfNull()) {
                auto strVal = reader.getString();
                auto gfStr =
                    column->getOverflowPages()->addString(strVal, overflowCursors[columnIdx]);
                column->setElement(nodeOffset, reinterpret_cast<uint8_t*>(&gfStr));
                if (structuredProperties[columnIdx].isPrimaryKey) {
                    nodeIDMap->set(strVal, nodeOffset);
                }
            }
        } break;
        case LIST: {
            if (!reader.skipTokenIfNull()) {
                auto listVal = reader.getList(*column->getDataType().childType);
                auto gfList =
                    column->getOverflowPages()->addList(listVal, overflowCursors[columnIdx]);
                column->setElement(nodeOffset, reinterpret_cast<uint8_t*>(&gfList));
            }
        } break;
        default:
            if (!reader.skipTokenIfNull()) {
                reader.skipToken();
            }
        }
    }
}

void InMemNodeBuilder::putUnstrPropsOfALineToLists(CSVReader& reader, node_offset_t nodeOffset,
    PageByteCursor& overflowPagesCursor,
    unordered_map<string, uint64_t>& unstrPropertiesNameToIdMap,
    InMemUnstructuredLists* unstrPropertyLists) {
    while (reader.hasNextToken()) {
        auto unstrPropertyString = reader.getString();
        auto unstrPropertyStringBreaker1 = strchr(unstrPropertyString, ':');
        *unstrPropertyStringBreaker1 = 0;
        auto propertyKeyId = (uint32_t)unstrPropertiesNameToIdMap.at(string(unstrPropertyString));
        auto unstrPropertyStringBreaker2 = strchr(unstrPropertyStringBreaker1 + 1, ':');
        *unstrPropertyStringBreaker2 = 0;
        auto dataType = Types::dataTypeFromString(string(unstrPropertyStringBreaker1 + 1));
        auto dataTypeSize = Types::getDataTypeSize(dataType);
        auto reversePos = InMemListsUtils::decrementListSize(*unstrPropertyLists->getListSizes(),
            nodeOffset, UnstructuredPropertyLists::UNSTR_PROP_HEADER_LEN + dataTypeSize);
        PageElementCursor pageElementCursor = InMemListsUtils::calcPageElementCursor(
            unstrPropertyLists->getListHeaders()->getHeader(nodeOffset), reversePos, 1, nodeOffset,
            *unstrPropertyLists->getListsMetadata(), false /*hasNULLBytes*/);
        PageByteCursor pageCursor{pageElementCursor.idx, pageElementCursor.pos};
        char* valuePtr = unstrPropertyStringBreaker2 + 1;
        switch (dataType.typeID) {
        case INT64: {
            auto intVal = TypeUtils::convertToInt64(valuePtr);
            unstrPropertyLists->setUnstructuredElement(pageCursor, propertyKeyId, dataType.typeID,
                (uint8_t*)(&intVal), &overflowPagesCursor);
        } break;
        case DOUBLE: {
            auto doubleVal = TypeUtils::convertToDouble(valuePtr);
            unstrPropertyLists->setUnstructuredElement(pageCursor, propertyKeyId, dataType.typeID,
                reinterpret_cast<uint8_t*>(&doubleVal), &overflowPagesCursor);
        } break;
        case BOOL: {
            auto boolVal = TypeUtils::convertToBoolean(valuePtr);
            unstrPropertyLists->setUnstructuredElement(pageCursor, propertyKeyId, dataType.typeID,
                reinterpret_cast<uint8_t*>(&boolVal), &overflowPagesCursor);
        } break;
        case DATE: {
            char* beginningOfDateStr = valuePtr;
            date_t dateVal = Date::FromCString(beginningOfDateStr, strlen(beginningOfDateStr));
            unstrPropertyLists->setUnstructuredElement(pageCursor, propertyKeyId, dataType.typeID,
                reinterpret_cast<uint8_t*>(&dateVal), &overflowPagesCursor);
        } break;
        case TIMESTAMP: {
            char* beginningOfTimestampStr = valuePtr;
            timestamp_t timestampVal =
                Timestamp::FromCString(beginningOfTimestampStr, strlen(beginningOfTimestampStr));
            unstrPropertyLists->setUnstructuredElement(pageCursor, propertyKeyId, dataType.typeID,
                reinterpret_cast<uint8_t*>(&timestampVal), &overflowPagesCursor);
        } break;
        case INTERVAL: {
            char* beginningOfIntervalStr = valuePtr;
            interval_t intervalVal =
                Interval::FromCString(beginningOfIntervalStr, strlen(beginningOfIntervalStr));
            unstrPropertyLists->setUnstructuredElement(pageCursor, propertyKeyId, dataType.typeID,
                reinterpret_cast<uint8_t*>(&intervalVal), &overflowPagesCursor);
        } break;
        case STRING: {
            unstrPropertyLists->setUnstructuredElement(pageCursor, propertyKeyId, dataType.typeID,
                reinterpret_cast<uint8_t*>(valuePtr), &overflowPagesCursor);
        } break;
        default:
            throw LoaderException("unsupported dataType while parsing unstructured property");
        }
    }
}

void InMemNodeBuilder::saveToFile() {
    logger->debug("Writing node structured columns to disk.");
    assert(!structuredColumns.empty());
    progressBar->addAndStartNewJob(
        "Saving structured columns and unstructured lists to disk for node: " + labelName,
        structuredColumns.size() + (unstrPropertyLists != nullptr));
    for (auto& column : structuredColumns) {
        taskScheduler.scheduleTask(LoaderTaskFactory::createLoaderTask(
            [&](InMemColumn* x, LoaderProgressBar* progressBar_) {
                x->saveToFile();
                progressBar_->incrementTaskFinished();
            },
            column.get(), progressBar));
    }
    if (unstrPropertyLists) {
        taskScheduler.scheduleTask(LoaderTaskFactory::createLoaderTask(
            [&](InMemLists* x, LoaderProgressBar* progressBar_) {
                x->saveToFile();
                progressBar_->incrementTaskFinished();
            },
            unstrPropertyLists.get(), progressBar));
    }
    taskScheduler.waitAllTasksToCompleteOrError();
    logger->debug("Done writing node structured columns to disk.");
}

} // namespace loader
} // namespace graphflow