#define NOMINMAX
#include <nan.h>
#include "terrainGenerator.h"

namespace paracosm {

class TerrainGeneratorObject : public Nan::ObjectWrap {
public:
    static NAN_MODULE_INIT(Init) {
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("TerrainGenerator").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(tpl, "getRoot", GetRoot);
        Nan::SetPrototypeMethod(tpl, "generateNode", GenerateNode);
        Nan::SetPrototypeMethod(tpl, "generateBoundingRegion", GenerateBoundingRegion);
        Nan::SetPrototypeMethod(tpl, "generateTerrain", GenerateTerrain);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("TerrainGenerator").ToLocalChecked(), 
            Nan::GetFunction(tpl).ToLocalChecked());
    }

private:
    explicit TerrainGeneratorObject(const TerrainGenerator::Config &config) : generator(TerrainGenerator(config)) { }

    ~TerrainGeneratorObject() { }

    const TerrainGenerator generator;

    static NAN_METHOD(New) {
        if (info.IsConstructCall()) {
            v8::Isolate* isolate = v8::Isolate::GetCurrent();
            v8::Handle<v8::Object> params = v8::Handle<v8::Object>::Cast(info[0]);
            TerrainGenerator::Config config;            
            config.persistence = params->Get(v8::String::NewFromUtf8(isolate, "persistence"))->NumberValue();
            config.maximumDisplacement = params->Get(v8::String::NewFromUtf8(isolate, "maximumDisplacement"))->NumberValue();
            config.generationDepth = (unsigned int) params->Get(v8::String::NewFromUtf8(isolate, "generationDepth"))->NumberValue();
            config.contentGenerationDepth = (unsigned int) params->Get(v8::String::NewFromUtf8(isolate, "contentGenerationDepth"))->NumberValue();
            v8::Handle<v8::Array> ellipsoid = v8::Handle<v8::Array>::Cast(params->Get(v8::String::NewFromUtf8(isolate, "ellipsoid")));
            config.ellipsoid[0] = ellipsoid->Get(0)->NumberValue();
            config.ellipsoid[1] = ellipsoid->Get(1)->NumberValue();
            config.ellipsoid[2] = ellipsoid->Get(2)->NumberValue();
            config.computeProperties();

            TerrainGeneratorObject* obj = new TerrainGeneratorObject(config);
            obj->Wrap(info.This());
            info.GetReturnValue().Set(info.This());
        } else {
            const int argc = 1;
            v8::Local<v8::Value> argv[argc] = {info[0]};
            v8::Local<v8::Function> cons = Nan::New(constructor());
            info.GetReturnValue().Set(cons->NewInstance(argc, argv));
        }
    }

    static NAN_METHOD(GetRoot) {
        TerrainGeneratorObject* obj = Nan::ObjectWrap::Unwrap<TerrainGeneratorObject>(info.Holder());
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::Handle<v8::Object> node = v8::Object::New(isolate);

        node->Set(v8::String::NewFromUtf8(isolate, "geometricError"), v8::Number::New(isolate, 100000000));
        node->Set(v8::String::NewFromUtf8(isolate, "refine"), v8::String::NewFromUtf8(isolate, "replace"));
        
        v8::Handle<v8::Object> boundingVolume = v8::Object::New(isolate);
        v8::Handle<v8::Array> sphere = v8::Array::New(isolate, 4);
        v8::Local<v8::Number> zero = v8::Number::New(isolate, 0);
        double maxRadius = std::max(std::max(obj->generator.config.ellipsoid[0], obj->generator.config.ellipsoid[1]), obj->generator.config.ellipsoid[2]);
        v8::Local<v8::Number> radius = v8::Number::New(isolate, maxRadius + obj->generator.config.maximumDisplacement);
        sphere->Set(0, zero);
        sphere->Set(1, zero);
        sphere->Set(2, zero);
        sphere->Set(3, radius);
        boundingVolume->Set(v8::String::NewFromUtf8(isolate, "sphere"), sphere);
        node->Set(v8::String::NewFromUtf8(isolate, "boundingVolume"), boundingVolume);

        v8::Handle<v8::Array> children = v8::Array::New(isolate, 8);
        children->Set(0, obj->generateNode(Hemisphere::WEST, 1, 1));
        children->Set(1, obj->generateNode(Hemisphere::WEST, 2, 1));
        children->Set(2, obj->generateNode(Hemisphere::WEST, 3, 1));
        children->Set(3, obj->generateNode(Hemisphere::WEST, 4, 1));
        children->Set(4, obj->generateNode(Hemisphere::EAST, 1, 1));
        children->Set(5, obj->generateNode(Hemisphere::EAST, 2, 1));
        children->Set(6, obj->generateNode(Hemisphere::EAST, 3, 1));
        children->Set(7, obj->generateNode(Hemisphere::EAST, 4, 1));
        node->Set(v8::String::NewFromUtf8(isolate, "children"), children);

        info.GetReturnValue().Set(node);
    }

    static NAN_METHOD(GenerateNode) {
        TerrainGeneratorObject* obj = Nan::ObjectWrap::Unwrap<TerrainGeneratorObject>(info.Holder());
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::Handle<v8::Object> params = v8::Handle<v8::Object>::Cast(info[0]);
        Hemisphere hemisphere = (Hemisphere)(int) params->Get(v8::String::NewFromUtf8(isolate, "hemisphere"))->NumberValue();
        unsigned int index = (unsigned int) params->Get(v8::String::NewFromUtf8(isolate, "index"))->NumberValue();
        unsigned int generationDepth = (unsigned int) params->Get(v8::String::NewFromUtf8(isolate, "generationDepth"))->NumberValue();

        v8::Handle<v8::Object> node = obj->generateNode(hemisphere, index, generationDepth);
        info.GetReturnValue().Set(node);
    }

    v8::Handle<v8::Object> generateNode(Hemisphere hemisphere, unsigned int index, unsigned int generationDepth) const {
        unsigned int depth = generator.getDepth(index);
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::Handle<v8::Object> node = v8::Object::New(isolate);

        v8::Handle<v8::Object> boundingVolume = v8::Object::New(isolate);
        v8::Handle<v8::Array> region = v8::Array::New(isolate, 6);
        BoundingRegion boundingRegion = generator.generateBoundingRegion(hemisphere, index);
        region->Set(0, v8::Number::New(isolate, boundingRegion.w));
        region->Set(1, v8::Number::New(isolate, boundingRegion.s));
        region->Set(2, v8::Number::New(isolate, boundingRegion.e));
        region->Set(3, v8::Number::New(isolate, boundingRegion.n));
        region->Set(4, v8::Number::New(isolate, boundingRegion.h1));
        region->Set(5, v8::Number::New(isolate, boundingRegion.h2));
        boundingVolume->Set(v8::String::NewFromUtf8(isolate, "region"), region);
        node->Set(v8::String::NewFromUtf8(isolate, "boundingVolume"), boundingVolume);

        double error = generator.calculateRegionError(boundingRegion);
        double terrainError = generator.calculateRemainingError(depth + generator.config.contentGenerationDepth);
        node->Set(v8::String::NewFromUtf8(isolate, "geometricError"), v8::Number::New(isolate, error + terrainError));
        node->Set(v8::String::NewFromUtf8(isolate, "refine"), v8::String::NewFromUtf8(isolate, "replace"));

        v8::Handle<v8::Object> content = v8::Object::New(isolate);
        node->Set(v8::String::NewFromUtf8(isolate, "content"), content);
        
        std::string filename = std::to_string((int) hemisphere) + std::string("_") + std::to_string(index);

        if (generationDepth != generator.config.generationDepth) {
            filename += std::string(".b3dm");
            content->Set(v8::String::NewFromUtf8(isolate, "url"), v8::String::NewFromUtf8(isolate, filename.c_str()));

            v8::Handle<v8::Array> children = v8::Array::New(isolate, 4);
            children->Set(0, generateNode(hemisphere, 4 * index + 1, generationDepth + 1));
            children->Set(1, generateNode(hemisphere, 4 * index + 2, generationDepth + 1));
            children->Set(2, generateNode(hemisphere, 4 * index + 3, generationDepth + 1));
            children->Set(3, generateNode(hemisphere, 4 * index + 4, generationDepth + 1));
            node->Set(v8::String::NewFromUtf8(isolate, "children"), children);
        } else {
            filename += std::string(".json");
            content->Set(v8::String::NewFromUtf8(isolate, "url"), v8::String::NewFromUtf8(isolate, filename.c_str()));
            node->Set(v8::String::NewFromUtf8(isolate, "children"), v8::Undefined(isolate));
        }
        
        return node;
    }

    static NAN_METHOD(GenerateBoundingRegion) {
        TerrainGeneratorObject* obj = Nan::ObjectWrap::Unwrap<TerrainGeneratorObject>(info.Holder());
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::Handle<v8::Object> params = v8::Handle<v8::Object>::Cast(info[0]);
        Hemisphere hemisphere = (Hemisphere)(int) params->Get(v8::String::NewFromUtf8(isolate, "hemisphere"))->NumberValue();
        unsigned int index = (unsigned int) params->Get(v8::String::NewFromUtf8(isolate, "index"))->NumberValue();
        
        BoundingRegion boundingRegion = obj->generator.generateBoundingRegion(hemisphere, index);
        v8::Handle<v8::Array> region = v8::Array::New(isolate, 6);
        region->Set(0, v8::Number::New(isolate, boundingRegion.w));
        region->Set(1, v8::Number::New(isolate, boundingRegion.s));
        region->Set(2, v8::Number::New(isolate, boundingRegion.e));
        region->Set(3, v8::Number::New(isolate, boundingRegion.n));
        region->Set(4, v8::Number::New(isolate, boundingRegion.h1));
        region->Set(5, v8::Number::New(isolate, boundingRegion.h2));

        info.GetReturnValue().Set(region);
    }

    static NAN_METHOD(GenerateTerrain) {
        TerrainGeneratorObject* obj = Nan::ObjectWrap::Unwrap<TerrainGeneratorObject>(info.Holder());
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::Handle<v8::Object> params = v8::Handle<v8::Object>::Cast(info[0]);
        Hemisphere hemisphere = (Hemisphere)(int) params->Get(v8::String::NewFromUtf8(isolate, "hemisphere"))->NumberValue();
        unsigned int index = (unsigned int) params->Get(v8::String::NewFromUtf8(isolate, "index"))->NumberValue();

        size_t length;
        char* data = obj->generator.generateTerrain(hemisphere, index, length);
        info.GetReturnValue().Set(Nan::NewBuffer(data, length, [](char* data, void* hint) {
            delete data;
        }, NULL).ToLocalChecked());
    }
 
    static inline Nan::Persistent<v8::Function>& constructor() {
        static Nan::Persistent<v8::Function> my_constructor;
        return my_constructor;
    }
};

NODE_MODULE(ProceduralWorld, TerrainGeneratorObject::Init)

}