/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * This class represents a Houdini Engine component in Unreal Engine. 
 *
 * The following section explains some of the designs and decisions made by us (regarding this class) and should be 
 * removed in the future from production code.
 *
 * One of the problems we've encountered with Unreal Engine design is that the editor UI (details panel) relies on 
 * class RTTI information. That is, the editor uses RTTI to enumerate all fields / properties of a component and 
 * constructs corresponding UI elements (sliders, input fields, and so forth) from that information.
 *
 * This RTTI information (and corresponding c++ stubs) is generated during the pre-build step by Unreal Engine. 
 * Unfortunately this means that each class assumes a fixed layout, which would not work in our case as 
 * each Houdini Digital Asset can have a variable number of parameters and even more, number of parameters can change
 * between the cooks. 
 *
 * There are multiple ways around this problem, including using arrays to store parameters, however
 * each approach has it's limitations ~ whether it's unnecessary complexity or issues with Blueprint integration. With
 * this in mind we chose a slightly different approach: after HDA is loaded, we enumerate its parameters and based on
 * that we generate new RTTI information and replace RTTI information generated by Unreal at runtime. 
 *
 * A bit more information regarding Unreal RTTI: for each UObject derived instance, class object is stored in Class 
 * member variable (UClass type). For each UPROPERTY, a UProperty instance is created and is stored in link list inside 
 * UClass object. UProperty has an internal offset, which is an offset in bytes (from the beginning of an object). 
 * Furthermore, this offset property is a signed 32 bit integer. 
 * 
 * Another problem is the space required by each component to store the data fetched from HDA parameters.
 * This information becomes available only once the asset is cooked and parameters are fetched. This problem can be 
 * solved in a few ways. First approach would be to request a larger memory block when component is created (but large
 * enough to accommodate all parameter data), use placement new to create the component at the beginning of the fetched
 * block and store parameter data past the end of the component data. This approach would work, but could potentially
 * cause problems if 3rd party user decided to store meta information in a similar way ~ effectively overwriting ours.
 * This is a pretty common use case. Second solution is to patch the Unreal engine ~ patch the offset property to be
 * 64 bit int. This way we could store properties outside the component and calculate necessary offsets from the 
 * beginning of an object. We think this is a reasonable approach, but would require a pull request to Epic. This is
 * on our TODO list. Our current temporary solution is to use a fixed component layout with 64k scratch space. We store
 * all property data (and patch corresponding offsets) inside that scratch space. Scratch space size is controlled by
 * HOUDINIENGINE_ASSET_SCRATCHSPACE_SIZE definition, which is defined in Plugin Build.cs file.
 *
 * Produced by:
 *      Damian Campeanu, Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once
#include "HAPI.h"
#include "HoudiniAssetComponent.generated.h"

class UClass;
class UProperty;
class UMaterial;
class FTransform;
class UHoudiniAsset;
class FPrimitiveSceneProxy;
class UHoudiniAssetComponent;
class FComponentInstanceDataCache;

struct FPropertyChangedEvent;

UCLASS(ClassGroup=(Rendering, Common), hidecategories=(Object,Activation,"Components|Activation"), ShowCategories=(Mobility), editinlinenew, meta=(BlueprintSpawnableComponent))
class HOUDINIENGINE_API UHoudiniAssetComponent : public UMeshComponent
{
	friend class FHoudiniMeshSceneProxy;
	GENERATED_UCLASS_BODY()

public:

	/** Houdini Asset associated with this component (except preview). Preview component will use PreviewHoudiniAsset instead. **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = HoudiniAsset, ReplicatedUsing = OnRep_HoudiniAsset)
	UHoudiniAsset* HoudiniAsset;

public:

	/** **/
	UFUNCTION()
	void OnRep_HoudiniAsset(UHoudiniAsset* OldHoudiniAsset);

	/** Change the Houdini Asset used by this component. **/
	UFUNCTION(BlueprintCallable, Category = "Components|HoudiniAsset")
	virtual void SetHoudiniAsset(UHoudiniAsset* NewHoudiniAsset);

public:

	/** Ticking function to check cooking / instatiation status. **/
	void TickHoudiniComponent();

	/** Used to differentiate native components from dynamic ones. **/
	void SetNative(bool InbIsNativeComponent);

	/** Return id of a Houdini asset. **/
	HAPI_AssetId GetAssetId() const;

	/** Set id of a Houdini asset. **/
	void SetAssetId(HAPI_AssetId InAssetId);

	/** Return current referenced Houdini asset. **/
	UHoudiniAsset* GetHoudiniAsset() const;

public: /** UObject methods. **/

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	virtual void BeginDestroy() OVERRIDE;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected: /** UActorComponent methods. **/

	virtual void OnComponentCreated() OVERRIDE;
	virtual void OnComponentDestroyed() OVERRIDE;

	virtual void OnRegister() OVERRIDE;
	virtual void OnUnregister() OVERRIDE;

	virtual void GetComponentInstanceData(FComponentInstanceDataCache& Cache) const OVERRIDE;
	virtual void ApplyComponentInstanceData(const FComponentInstanceDataCache& Cache) OVERRIDE;

private: /** UPrimitiveComponent methods. **/

	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;

private: /** UMeshComponent methods. **/

	virtual int32 GetNumMaterials() const OVERRIDE;

private: /** USceneComponent methods. **/

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const OVERRIDE;

protected:

	/** Patch RTTI : patch class information for this component's class based on given Houdini Asset. **/
	void ReplaceClassInformation();

private:

	/** Patch RTTI : translate asset parameters to class properties and insert them into a given class instance. **/
	bool ReplaceClassProperties(UClass* ClassInstance);

	/** Patch RTTI: remove generated properties from class information object. **/
	void RemoveClassProperties(UClass* ClassInstance);

	/** Patch RTTI : patch class object. **/
	void ReplaceClassObject(UClass* ClassObjectNew);

	/** Patch RTTI : replace property offset data. **/
	void ReplacePropertyOffset(UProperty* Property, int Offset);

	/** Patch RTTI : Create various properties. **/
	UProperty* CreatePropertyInt(UClass* ClassInstance, const FName& Name, int Count, const int32* Value, uint32& Offset);
	UProperty* CreatePropertyFloat(UClass* ClassInstance, const FName& Name, int Count, const float* Value, uint32& Offset);
	UProperty* CreatePropertyToggle(UClass* ClassInstance, const FName& Name, int Count, const int32* bValue, uint32& Offset);
	UProperty* CreatePropertyColor(UClass* ClassInstance, const FName& Name, int Count, const float* Value, uint32& Offset);

	/** Set parameter values which have changed. **/
	void SetChangedParameterValues();

	/** Helper function to compute proper alignment boundary at a given offset for a specified type. **/
	template <typename TType> TType* ComputeOffsetAlignmentBoundary(uint32 Offset) const;

	/** Update rendering information. **/
	void UpdateRenderingInformation();

	/** Refresh editor's detail panel and update properties. **/
	void UpdateEditorProperties();

	/** Start ticking. **/
	void StartHoudiniTicking();

	/** Stop ticking. **/
	void StopHoudiniTicking();

public:

	/** Some RTTI classes which are used during property construction. **/
	static UScriptStruct* ScriptStructColor;

protected:

	/** Triangle data used for rendering in viewport / preview window. **/
	TArray<FHoudiniMeshTriangle> HoudiniMeshTriangles;

	/** Array of properties that have changed. Will force object recook. **/
	TSet<UProperty*> ChangedProperties;

	/** Notification used by this component. **/
	TWeakPtr<SNotificationItem> NotificationPtr;

	/** Bounding volume information for current geometry. **/
	FBoxSphereBounds HoudiniMeshSphereBounds;

	/** Synchronization primitive used to control access to geometry. **/
	FCriticalSection CriticalSectionTriangles;

	/** GUID used to track asynchronous cooking requests. **/
	FGuid HapiGUID;

	/** Timer delegate, used to simulate ticks. **/
	FTimerDelegate TimerDelegate;

	/** Id of corresponding Houdini asset. **/
	HAPI_AssetId AssetId;

	/** Is set to true when this component is native and false is when it is dynamic. **/
	bool bIsNativeComponent;

	/** Is set to true when this component belongs to a preview actor. **/
	bool bIsPreviewComponent;

private:

	/** Marker ~ beginning of scratch space. **/
	uint64 ScratchSpaceMarker;

	/** Scratch space buffer ~ used to store data for each property. **/
	char ScratchSpaceBuffer[HOUDINIENGINE_ASSET_SCRATCHSPACE_SIZE];
};


template <typename TType>
TType*
UHoudiniAssetComponent::ComputeOffsetAlignmentBoundary(uint32 Offset) const
{
	return Align<TType*>((TType*)(((char*) this) + Offset), ALIGNOF(TType));
}
