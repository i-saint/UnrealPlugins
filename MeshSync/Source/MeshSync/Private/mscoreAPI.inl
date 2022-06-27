#pragma region Server
msAPI(msGetPluginVersionStr, const char*);
msAPI(msGetProtocolVersion, int, );
msAPI(msServerStart, ms::Server*, const ms::ServerSettings* settings);
msAPI(msServerStop, void, ms::Server* server);
msAPI(msServerGetSplitUnit, uint32_t, ms::Server* server);
msAPI(msServerSetSplitUnit, void, ms::Server* server, uint32_t v);
msAPI(msServerGetMaxBoneInfluence, int, ms::Server* server);
msAPI(msServerSetMaxBoneInfluence, void, ms::Server* server, int v);
msAPI(msServerGetZUpCorrectionMode, ms::ZUpCorrectionMode, ms::Server* server);
msAPI(msServerSetZUpCorrectionMode, void, ms::Server* server, ms::ZUpCorrectionMode v);
msAPI(msServerGetNumMessages, int, ms::Server* server);
msAPI(msServerProcessMessages, int, ms::Server* server, msMessageHandler handler);
msAPI(msServerBeginServe, void, ms::Server* server);
msAPI(msServerEndServe, void, ms::Server* server);
msAPI(msServerServeTransform, void, ms::Server* server, ms::Transform* data);
msAPI(msServerServeCamera, void, ms::Server* server, ms::Camera* data);
msAPI(msServerServeLight, void, ms::Server* server, ms::Light* data);
msAPI(msServerServeMesh, void, ms::Server* server, ms::Mesh* data);
msAPI(msServerServeMaterial, void, ms::Server* server, ms::Material* data);
msAPI(msServerServeTexture, void, ms::Server* server, ms::Texture* data);
msAPI(msServerSetFileRootPath, void, ms::Server* server, const char* path);
msAPI(msServerAllowPublicAccess, void, ms::Server* server, const bool access);
msAPI(msServerIsPublicAccessAllowed, bool, ms::Server* server);
msAPI(msServerSetScreenshotFilePath, void, ms::Server* server, const char* path);
msAPI(msServerNotifyPoll, void, ms::Server* server, ms::PollMessage::PollType t);
#pragma endregion


#pragma region Messages
msAPI(msMessageGetSessionID, int, ms::Message*);
msAPI(msMessageGetMessageID, int, ms::Message* self);
msAPI(msGetGetFlags, ms::GetFlags, ms::GetMessage* self);
msAPI(msGetGetBakeSkin, int, ms::GetMessage* self);
msAPI(msGetGetBakeCloth, int, ms::GetMessage* self);
msAPI(msSetGetSceneData, ms::Scene*, ms::SetMessage* self);
msAPI(msDeleteGetNumEntities, int, ms::DeleteMessage* self);
msAPI(msDeleteGetEntity, ms::Identifier*, ms::DeleteMessage* self, int i);
msAPI(msDeleteGetNumMaterials, int, ms::DeleteMessage* self);
msAPI(msDeleteGetMaterial, ms::Identifier*, ms::DeleteMessage* self, int i);
msAPI(msDeleteGetNumInstances, int, ms::DeleteMessage* self);
msAPI(msDeleteGetInstance, ms::Identifier*, ms::DeleteMessage* self, int i);
msAPI(msFenceGetType, ms::FenceMessage::FenceType, ms::FenceMessage* self);
msAPI(msTextGetText, const char*, ms::TextMessage* self);
msAPI(msTextGetType, ms::TextMessage::Type, ms::TextMessage* self);
msAPI(msQueryGetType, ms::QueryMessage::QueryType, ms::QueryMessage* self);
msAPI(msQueryFinishRespond, void, ms::QueryMessage* self);
msAPI(msQueryAddResponseText, void, ms::QueryMessage* self, const char* text);
#pragma endregion


#pragma region Transform
msAPI(msTransformGetDataFlags, uint32_t, const ms::Transform* self);
msAPI(msTransformGetType, ms::EntityType, const ms::Transform* self);
msAPI(msTransformGetID, int, const ms::Transform* self);
msAPI(msTransformGetHostID, int, const ms::Transform* self);
msAPI(msTransformGetIndex, int, const ms::Transform* self);
msAPI(msTransformGetPath, const char*, const ms::Transform* self);
msAPI(msTransformGetPosition, mu::float3, const ms::Transform* self);
msAPI(msTransformGetRotation, mu::quatf, const ms::Transform* self);
msAPI(msTransformGetScale, mu::float3, const ms::Transform* self);
msAPI(msTransformGetVisibility, uint32_t, const ms::Transform* self);
msAPI(msTransformGetReference, const char*, const ms::Transform* self);
msAPI(msTransformGetNumUserProperties, int, const ms::Transform* self);
msAPI(msTransformGetUserProperty, const ms::Variant*, const ms::Transform* self, int i);
msAPI(msTransformFindUserProperty, const ms::Variant*, const ms::Transform* self, const char* name);
#pragma endregion


#pragma region Camera
msAPI(msCameraGetDataFlags, uint32_t, const ms::Camera* self);
msAPI(msCameraIsOrtho, bool, const ms::Camera* self);
msAPI(msCameraGetFov, float, const ms::Camera* self);
msAPI(msCameraGetNearPlane, float, const ms::Camera* self);
msAPI(msCameraGetFarPlane, float, const ms::Camera* self);
msAPI(msCameraGetFocalLength, float, const ms::Camera* self);
msAPI(msCameraGetSensorSize, mu::float2, const ms::Camera* self);
msAPI(msCameraGetLensShift, mu::float2, const ms::Camera* self);
msAPI(msCameraGetViewMatrix, mu::float4x4, const ms::Camera* self);
msAPI(msCameraGetProjMatrix, mu::float4x4, const ms::Camera* self);
#pragma endregion


#pragma region Light
msAPI(msLightGetDataFlags, uint32_t, const ms::Light* self);
msAPI(msLightGetType, ms::Light::LightType, const ms::Light* self);
msAPI(msLightGetShadowType, ms::Light::ShadowType, const ms::Light* self);
msAPI(msLightGetColor, mu::float4, const ms::Light* self);
msAPI(msLightGetIntensity, float, const ms::Light* self);
msAPI(msLightGetRange, float, const ms::Light* self);
msAPI(msLightGetSpotAngle, float, const ms::Light* self);
#pragma endregion


#pragma region Mesh
msAPI(msMeshGetDataFlags, uint32_t, const ms::Mesh* self);
msAPI(msMeshGetNumPoints, int, const ms::Mesh* self);
msAPI(msMeshGetNumIndices, int, const ms::Mesh* self);
msAPI(msMeshGetNumCounts, int, const ms::Mesh* self);
msAPI(msMeshReadPoints, void, const ms::Mesh* self, mu::float3* dst);
msAPI(msMeshReadNormals, void, const ms::Mesh* self, mu::float3* dst);
msAPI(msMeshReadTangents, void, const ms::Mesh* self, mu::float4* dst);
msAPI(msMeshReadUV, void, const ms::Mesh* self, mu::float2* dst, int index);
msAPI(msMeshReadColors, void, const ms::Mesh* self, mu::float4* dst);
msAPI(msMeshReadVelocities, void, const ms::Mesh* self, mu::float3* dst);
msAPI(msMeshReadIndices, void, const ms::Mesh* self, int* dst);
msAPI(msMeshReadCounts, void, const ms::Mesh* self, int* dst);
msAPI(msMeshGetPointsPtr, const mu::float3*, const ms::Mesh* self);
msAPI(msMeshGetNormalsPtr, const mu::float3*, const ms::Mesh* self);
msAPI(msMeshGetTangentsPtr, const mu::float4*, const ms::Mesh* self);
msAPI(msMeshGetUVPtr, const mu::float2*, const ms::Mesh* self, int index);
msAPI(msMeshGetColorsPtr, const mu::float4*, const ms::Mesh* self);
msAPI(msMeshGetVelocitiesPtr, const mu::float3* , const ms::Mesh* self);
msAPI(msMeshGetIndicesPtr, const int*, const ms::Mesh* self);
msAPI(msMeshGetCountsPtr, const int*, const ms::Mesh* self);
msAPI(msMeshGetNumSubmeshes, int, const ms::Mesh* self);
msAPI(msMeshGetSubmesh, const ms::SubmeshData*, const ms::Mesh* self, int i);
msAPI(msMeshGetBounds, ms::Bounds, const ms::Mesh* self);

msAPI(msMeshReadBoneWeights4, void, const ms::Mesh* self, mu::Weights4* dst);
msAPI(msMeshReadBoneCounts, void, const ms::Mesh* self, uint8_t* dst);
msAPI(msMeshReadBoneWeightsV, void, const ms::Mesh* self, mu::Weights1* dst);
msAPI(msMeshGetBoneCountsPtr, const uint8_t*, const ms::Mesh* self);
msAPI(msMeshGetBoneWeightsVPtr, const mu::Weights1*, const ms::Mesh* self);
msAPI(msMeshGetNumBones, int, const ms::Mesh* self);
msAPI(msMeshGetNumBoneWeights, int, const ms::Mesh* self);
msAPI(msMeshGetRootBonePath, const char*, const ms::Mesh* self);
msAPI(msMeshGetBonePath, const char*, const ms::Mesh* self, int i);
msAPI(msMeshReadBindPoses, void, const ms::Mesh* self, mu::float4x4* v);
msAPI(msMeshGetNumBlendShapes, int, const ms::Mesh* self);
msAPI(msMeshGetBlendShapeData, const ms::BlendShapeData*, const ms::Mesh* self, int i);

msAPI(msSubmeshGetNumIndices, int, const ms::SubmeshData* self);
msAPI(msSubmeshReadIndices, void, const ms::SubmeshData* self, const ms::Mesh* mesh, int* dst);
msAPI(msSubmeshGetMaterialID, int, const ms::SubmeshData* self);
msAPI(msSubmeshGetTopology, ms::Topology, const ms::SubmeshData* self);

msAPI(msBlendShapeGetName, const char*, const ms::BlendShapeData* self);
msAPI(msBlendShapeGetWeight, float, const ms::BlendShapeData* self);
msAPI(msBlendShapeGetNumFrames, int, const ms::BlendShapeData* self);
msAPI(msBlendShapeGetFrameWeight, float, const ms::BlendShapeData* self, int f);
msAPI(msBlendShapeReadPoints, void, const ms::BlendShapeData* self, int f, mu::float3* dst);
msAPI(msBlendShapeReadNormals, void, const ms::BlendShapeData* self, int f, mu::float3* dst);
msAPI(msBlendShapeReadTangents, void, const ms::BlendShapeData* self, int f, mu::float3* dst);
#pragma endregion


#pragma region Points
msAPI(msPointsGetFlags, uint32_t, const ms::Points* self);
msAPI(msPointsGetBounds, ms::Bounds, const ms::Points* self);
msAPI(msPointsGetNumPoints, int, const ms::Points* self, mu::float3* dst);
msAPI(msPointsReadPoints, void, const ms::Points* self, mu::float3* dst);
msAPI(msPointsReadRotations, void, const ms::Points* self, mu::quatf* dst);
msAPI(msPointsReadScales, void, const ms::Points* self, mu::float3* dst);
msAPI(msPointsReadVelocities, void, const ms::Points* self, mu::float3* dst);
msAPI(msPointsReadColors, void, const ms::Points* self, mu::float4* dst);
msAPI(msPointsReadIDs, void, const ms::Points* self, int* dst);
#pragma endregion


#pragma region InstanceInfo
msAPI(msInstanceInfoGetPath, const char*, const ms::InstanceInfo* self);
msAPI(msInstanceInfoGetParentPath, const char*, const ms::InstanceInfo* self);
msAPI(msInstanceInfoPropGetArrayLength, int, const ms::InstanceInfo* self);
msAPI(msInstanceInfoCopyTransforms, void, const ms::InstanceInfo* self, mu::float4x4* dst);
#pragma endregion


#pragma region Scene
msAPI(msSceneGetNumAssets, int, const ms::Scene* self);
msAPI(msSceneGetNumEntities, int, const ms::Scene* self);
msAPI(msSceneGetNumConstraints, int, const ms::Scene* self);
msAPI(msSceneGetNumInstanceInfos, int, const ms::Scene* self);
msAPI(msSceneGetNumInstanceMeshes, int, const ms::Scene* self);
msAPI(msSceneGetAsset, ms::Asset*, const ms::Scene* self, int i);
msAPI(msSceneGetEntity, ms::Transform*, const ms::Scene* self, int i);
msAPI(msSceneGetConstraint, ms::Constraint*, const ms::Scene* self, int i);
msAPI(msSceneGetInstanceInfo, ms::InstanceInfo*, const ms::Scene* self, int i);
msAPI(msSceneGetInstanceMesh, ms::Transform*, const ms::Scene* self, int i);
msAPI(msSceneSubmeshesHaveUniqueMaterial, bool, const ms::Scene* self);
#pragma endregion


#pragma region Misc
msAPI(msGetTime, uint64_t);
#pragma endregion
