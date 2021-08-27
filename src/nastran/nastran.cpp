#include <vsgXchange/nastran.h>
#include <vsg/all.h>
#include <iostream>
#include <regex>
#include <fstream>
#include <map>

using namespace vsgXchange;

#define GLSL450(src) "#version 450\n #extension GL_ARB_separate_shader_objects : enable\n\n" #src

const char* vertSource = GLSL450(

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 modelview;
} pc;

layout(location = 0) in vec4 nastranVertex; ///(X,Y,Z,Temp)

layout(location = 0) out vec3 Temp;


out gl_PerVertex{
    vec4 gl_Position;
};


void main() {
    vec3 Vertex = nastranVertex.xyz;
    float Color = nastranVertex.w;
    gl_Position = (pc.projection * pc.modelview) * vec4(Vertex, 1.0f);
    Temp = vec3(Color);
}
);

const char* fragSource = GLSL450(

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 Temp;

void main() {
    outColor = vec4(Temp, 1.0f);
}
);

bool debugOutput = false;


void normalizeTemperatures(std::vector<float>& temperatures) 
{
    float maxTemp = *std::max_element(temperatures.begin(), temperatures.end());
    float minTemp = *std::min_element(temperatures.begin(), temperatures.end());

    if (debugOutput) {
        std::cout << "maxTemp: " << maxTemp << std::endl;
        std::cout << "minTemp: " << minTemp << std::endl;
    }
    
    //Normalizing Temps between 0 and 1
    for (int i = 0; i < temperatures.size(); i++)
    {
        float normalizedTemp = (temperatures[i] - minTemp) / (maxTemp - minTemp);
        if(debugOutput) std::cout << "normalize temp: " << temperatures[i] << " -> " << normalizedTemp << std::endl;
        temperatures[i] = normalizedTemp;
    }

}

void parseGridsToIDAndVec3Vec(const std::string& line, std::vector<vsg::vec3>& gridList, std::vector<int>& idList) 
{

    std::regex regex(",");

    std::vector<std::string> out(
        std::sregex_token_iterator(line.begin(), line.end(), regex, -1),
        std::sregex_token_iterator()
    );

    int id = 0;
    vsg::vec3 vec3Value(0.f, 0.f, 0.f);

    int index = 0;
    for (auto& s : out) {
        switch (index)
        {
        case 1: 
            id = std::stoi(s);
            idList.push_back(id);
            break;

        case 3: 
            vec3Value.x = std::stof(s); 
            break;
        case 4: 
            vec3Value.y = std::stof(s); 
            break;
        case 5: 
            vec3Value.z = std::stof(s); 
            break;
        }
        index++;
    }
    gridList.push_back(vec3Value);

    if (debugOutput) std::cout << "Grid: [" << id << "] " << vec3Value[0] << " " << vec3Value[1] << " " << vec3Value[2] << std::endl;
}

void parseTRIAToIDList(const std::string& line, std::vector<int>& idArray) 
{
    std::regex regex(",");

    std::vector<std::string> out(
        std::sregex_token_iterator(line.begin(), line.end(), regex, -1),
        std::sregex_token_iterator()
    );

    if (debugOutput) std::cout << "TRIA: ";

    int index = 0;
    for (auto& s : out) {
        if (index <= 5 && index > 2) {
            int id = std::stoi(s);
            idArray.push_back(id);
            if (debugOutput) std::cout << id << " ";
        }
        index++;
    }
    if (debugOutput) std::cout << std::endl;
}

void parseQUADToIDList(const std::string& line, std::vector<int>& idArray) 
{
    std::regex regex(",");

    std::vector<int> unfoldIndices{0,1,2,2,3,0};
    std::vector<int> quadIndices;

    std::vector<std::string> out(
        std::sregex_token_iterator(line.begin(), line.end(), regex, -1),
        std::sregex_token_iterator()
    );

    if (debugOutput) std::cout << "QUAD: ";

    int index = 0;
    for (auto& s : out) {
        if (index <= 6 && index > 2) {
            int id = std::stoi(s);
            quadIndices.push_back(id); 
            if (debugOutput) std::cout << id << " ";
        }
        index++;
    }

    if (debugOutput) std::cout << " -> ";

    for (auto unfoldIndex : unfoldIndices)
    {
        int unfoldedID = quadIndices[unfoldIndex];
        idArray.push_back(unfoldedID);
        if (debugOutput) std::cout << unfoldedID << " ";
    }
    if (debugOutput) std::cout << std::endl;
}

void parseTempToIDandFloatVec(const std::string& line, std::vector<float>& temperatureList, std::vector<int>& idList) 
{
    std::regex regex(",");

    std::vector<std::string> out(
        std::sregex_token_iterator(line.begin(), line.end(), regex, -1),
        std::sregex_token_iterator()
    );

    int id = 0;
    float temperature = 0.f;

    int index = 0;
    for (auto& s : out) {
        switch (index)
        {

        case 2: 
            id = std::stoi(s);
            idList.push_back(id);
            break;
        
        case 3: 
            temperature = std::stof(s);
            temperatureList.push_back(temperature);
            break;
        

        }
        index++;
    }

    if (debugOutput) std::cout << "Temp: [" << id << "] " << temperature << std::endl;
}

nastran::nastran() 
{
    debugOutput = enableDebugOutput;
}

vsg::ref_ptr<vsg::Object> nastran::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    auto ext = vsg::lowerCaseFileExtension(filename);
    if (ext != "nas" || !vsg::fileExists(filename)) return { };
    

    std::ifstream stream(filename.c_str(), std::ios::in);
    if (!stream) return { };
    return read(stream, options);

    return vsg::ref_ptr<vsg::Object>();
}

void printVec3(int index, const vsg::vec3 input)
{
    if (debugOutput) std::cout << "[" << index << "] " << input.x << " / " << input.y << " / " << input.z << std::endl;
}

vsg::ref_ptr<vsg::Object> nastran::read(std::istream& stream, vsg::ref_ptr<const vsg::Options> options) const
{
    std::string line;
  
    //values GRID,VertexIndex,,X,Y,Z,,,, -> vec3(X,Y,Z)
    std::vector<vsg::vec3> gridList;
    //keys GRID,VertexIndex,,X,Y,Z,,,, -> VertexIndex
    std::vector<int> gridIDs;

    //values TEMP,TempNum,TempIndex,TempValue,,,,,, -> (TempValue)
    std::vector<float> temperatureList;
    //keys TEMP,TempNum,TempIndex,TempValue,,,,,, -> (TempIndex)
    std::vector<int> temperatureIDs;

    //Simple list of all Trias and Quads.
    //"Unfolded" because Quads get parsed into 2 triangles
    //Still contains the IDs, has to be converted to an IndexBuffer
    std::vector<int> unfoldedIDList;
    
    while (getline(stream, line))
    {
        if (line.find("GRID,") != std::string::npos)
        {
            parseGridsToIDAndVec3Vec(line, gridList, gridIDs);
        }
        else if (line.find("CTRIA3,") != std::string::npos)
        {
            parseTRIAToIDList(line, unfoldedIDList);
        }
        else if (line.find("QUAD4,") != std::string::npos)
        {
            parseQUADToIDList(line, unfoldedIDList);
        }
        else if (line.find("TEMP,") != std::string::npos)
        {
            parseTempToIDandFloatVec(line, temperatureList, temperatureIDs);
        }
    }

    
    //Do TempIndices and VertexIndices match?
    std::sort(gridIDs.begin(), gridIDs.end());
    std::sort(temperatureIDs.begin(), temperatureIDs.end());

    if (temperatureIDs != gridIDs) {
        if (debugOutput) std::cout << "Broken Nastran file detected: Vertex Indices and Temperature Indices do not have a Bijective mapping. Skipping load." << std::endl;
        return { };
    }

    //Normalize Temperatures
    normalizeTemperatures(temperatureList);

    //Normalized map of ordered ids, e.g. nastran grids are 5, 10, 23 -> map to -> 0, 1, 2, to fit into an index buffer
    int normalizeIDIndex = 0;
    std::map<int, int> gridToIndexMap;
    for (auto gridID : gridIDs) {
        std::pair<int, int> normalizedPair;
        normalizedPair.first = gridID;
        normalizedPair.second = normalizeIDIndex;
        gridToIndexMap.insert(normalizedPair);
        if (debugOutput) std::cout << "GridMap: " << gridID << " -> " << normalizeIDIndex << std::endl;
        normalizeIDIndex++;
    }


    //create a nastran List structured like this -> List<vec4(X, Y, Z, Temp)>
    //just merge the gridList and temperatureList
    std::vector<vsg::vec4> nastranList;
    int nastranListIndex = 0;
    for (auto gridItem : gridList)
    {
        vsg::vec4 nastranItem(gridItem.x, gridItem.y, gridItem.z, temperatureList[nastranListIndex]);
        nastranList.push_back(nastranItem);
        if (debugOutput) std::cout << "NAS [" << nastranListIndex << "]: " << gridItem.x << " / " << gridItem.y << " / " << gridItem.z << " Normalized Temperature: " << temperatureList[nastranListIndex] << std::endl;
        nastranListIndex++;
    }


    //convert idList to indexBuffer
    for (int i = 0; i < unfoldedIDList.size(); i++) {
        unfoldedIDList[i] = gridToIndexMap[unfoldedIDList[i]];
        if (debugOutput) std::cout << "Map Grid ID [" << i << "] -> to IndexBufferID -> " << unfoldedIDList[i] << std::endl;
    }

    if (debugOutput) std::cout << "unfolded indexBuffer size: " << unfoldedIDList.size() << std::endl;

    //Now everythings here
    //Vertex Buffer -> nastranList = List<vec4(X, Y, Z, Temp)>
    //Index Buffer -> unfoldedIDList
    //Convert to valid vsg::Data
    
    vsg::ref_ptr<vsg::vec4Array> vsg_vertices;
    vsg::ref_ptr<vsg::intArray> vsg_indices;

    auto convertedVertexArray = vsg::vec4Array::create(static_cast<uint32_t>(nastranList.size()));
    std::copy(nastranList.begin(), nastranList.end(), convertedVertexArray->data());
    vsg_vertices = convertedVertexArray;

    auto convertedIndex = vsg::intArray::create(static_cast<uint32_t>(unfoldedIDList.size()));
    std::copy(unfoldedIDList.begin(), unfoldedIDList.end(), convertedIndex->data());
    vsg_indices = convertedIndex;


    vsg::ref_ptr<vsg::ShaderStage> vertexShader = vsg::ShaderStage::create(VK_SHADER_STAGE_VERTEX_BIT, "main", vertSource);
    vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragSource);
    if (!vertexShader || !fragmentShader)
    {
        if (debugOutput) std::cout << "Could not create shaders." << std::endl;
        return { };
    }

    vsg::PushConstantRanges pushConstantRanges{ //
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls automatically provided by the VSG's DispatchTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec4), VK_VERTEX_INPUT_RATE_VERTEX}, // nastran vertex
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0}, // vertex data
    };

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create() };

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{ }, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{ vertexShader, fragmentShader }, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);


    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto stategroup = vsg::StateGroup::create();
    stategroup->add(bindGraphicsPipeline);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    stategroup->addChild(transform);

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{ vsg_vertices }));
    drawCommands->addChild(vsg::BindIndexBuffer::create(vsg_indices));
    drawCommands->addChild(vsg::DrawIndexed::create(vsg_indices->size(), 1, 0, 0, 0));

    // add drawCommands to transform
    transform->addChild(drawCommands);

    std::cout << "Nastran file loaded" << std::endl;

    return stategroup;
}


bool nastran::getFeatures(Features& features) const
{
    features.extensionFeatureMap["nas"] = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM);
    return true;
}