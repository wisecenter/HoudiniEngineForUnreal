/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Paths.h"

#include "HoudiniEngineRuntimeCommon.h"
#include "HoudiniEngineRuntimeUtils.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniOutput.h"
#include "HoudiniPluginSerializationVersion.h"
#include "HoudiniAssetStateTypes.h"
#include "IHoudiniAssetStateEvents.h"

#include "Engine/EngineTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

#include "HoudiniAssetComponent.generated.h"

class UHoudiniAsset;
class UHoudiniParameter;
class UHoudiniInput;
class UHoudiniOutput;
class UHoudiniHandleComponent;
class UHoudiniPDGAssetLink;

UENUM()
enum class EHoudiniStaticMeshMethod : uint8
{
	// Static Meshes will be generated by using Raw Meshes.
	RawMesh_DELETED,
	// Static Meshes will be generated by using Mesh Descriptions.
	FMeshDescription,
	// Always build Houdini Proxy Meshes (dev)
	UHoudiniStaticMesh,
};

UENUM()
enum class EHoudiniBakeAfterNextCook : uint8
{
	// Do not bake after cook
	Disabled,
	// Always bake after cook if cook was successful,
	Always,
	// Bake after the next successful cook, then reset to Disabled.
	Once
};

class UHoudiniAssetComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FHoudiniAssetEvent, UHoudiniAsset*);
DECLARE_MULTICAST_DELEGATE_OneParam(FHoudiniAssetComponentEvent, UHoudiniAssetComponent*)

UCLASS(ClassGroup = (Rendering, Common), hidecategories = (Object, Activation, "Components|Activation"), ShowCategories = (Mobility), editinlinenew)
class HOUDINIENGINERUNTIME_API UHoudiniAssetComponent : public UPrimitiveComponent, public IHoudiniAssetStateEvents
{
	GENERATED_UCLASS_BODY()

	// Declare translators as friend so they can easily directly modify
	// Inputs, outputs and parameters
	friend class FHoudiniEngineManager;
	friend struct FHoudiniOutputTranslator;
	friend struct FHoudiniInputTranslator;
	friend struct FHoudiniSplineTranslator;
	friend struct FHoudiniParameterTranslator;
	friend struct FHoudiniPDGManager;
	friend struct FHoudiniHandleTranslator;

#if WITH_EDITORONLY_DATA
	friend class FHoudiniAssetComponentDetails;
#endif
	friend class FHoudiniEditorEquivalenceUtils;
	
public:

	// Declare the delegate that is broadcast when RefineMeshesTimer fires
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRefineMeshesTimerDelegate, UHoudiniAssetComponent*);
	// Delegate for when EHoudiniAssetState changes from InFromState to InToState on a Houdini Asset Component (InHAC).
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAssetStateChangeDelegate, UHoudiniAssetComponent*, const EHoudiniAssetState, const EHoudiniAssetState);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreInstantiationDelegate, UHoudiniAssetComponent*);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreCookDelegate, UHoudiniAssetComponent*);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPostCookDelegate, UHoudiniAssetComponent*, bool);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPostBakeDelegate, UHoudiniAssetComponent*, bool);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPostOutputProcessingDelegate, UHoudiniAssetComponent*, bool);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPreOutputProcessingDelegate, UHoudiniAssetComponent*, bool);

	virtual ~UHoudiniAssetComponent();

	virtual void Serialize(FArchive & Ar) override;

	// Called after the C++ constructor and after the properties have been initialized, including those loaded from config.
	// This is called before any serialization or other setup has happened.
	virtual void PostInitProperties() override;

	// Returns the Owner actor / HAC name
	FString	GetDisplayName() const;

	// Indicates if the HAC needs to be updated
	bool NeedUpdate() const;

	// Indicates if any of the HAC's output components needs to be updated (no recook needed)
	bool NeedOutputUpdate() const;

	// Check whether any inputs / outputs / parameters have made blueprint modifications.
	bool NeedBlueprintStructureUpdate() const;
	bool NeedBlueprintUpdate() const;

	// Prevents automatic triggering of updates on this HAC in its current state.
	// This is to prevent endless cook/instantiation loops when an issue happens
	void PreventAutoUpdates();

	// Try to find one of our parameter that matches another (name, type, size and enabled)
	UHoudiniParameter* FindMatchingParameter(UHoudiniParameter* InOtherParam);

	// Try to find one of our input that matches another one (name, isobjpath, index / parmId)
	UHoudiniInput* FindMatchingInput(UHoudiniInput* InOtherInput);

	// Try to find one of our handle that matches another one (name and handle type)
	UHoudiniHandleComponent* FindMatchingHandle(UHoudiniHandleComponent* InOtherHandle);

	// Finds a parameter by name
	UHoudiniParameter* FindParameterByName(const FString& InParamName);

	// Returns True if the component has at least one mesh output of class U
	template <class U>
	bool HasMeshOutputObjectOfClass() const;

	// Returns True if the component has at least one mesh output with a current proxy
	bool HasAnyCurrentProxyOutput() const;

	// Returns True if the component has at least one proxy mesh output (not necessarily current/displayed)
	bool HasAnyProxyOutput() const;

	// Returns True if the component has at least one non-proxy output component amongst its outputs
	bool HasAnyOutputComponent() const;

	// Returns true if the component has InOutputObjectToFind in its output object
	bool HasOutputObject(UObject* InOutputObjectToFind) const;

	/** Getter for the cached world pointer, will return null if the component is not actually spawned in a level */
	virtual UWorld* GetHACWorld() const;

	//------------------------------------------------------------------------------------------------
	// Accessors
	//------------------------------------------------------------------------------------------------
	UHoudiniAsset * GetHoudiniAsset() const;
	int32 GetAssetId() const { return AssetId; };
	EHoudiniAssetState GetAssetState() const { return AssetState; };
//	FString GetAssetStateAsString() const { return FHoudiniEngineRuntimeUtils::EnumToString(TEXT("EHoudiniAssetState"), GetAssetState()); };

	virtual FString GetHoudiniAssetName() const;

	EHoudiniAssetStateResult GetAssetStateResult() const { return AssetStateResult; };
	FGuid GetHapiGUID() const { return HapiGUID; };
	FString GetHapiAssetName() const { return HapiAssetName; };
	FGuid GetComponentGUID() const { return ComponentGUID; };

	int32 GetNumInputs() const { return Inputs.Num(); };
	int32 GetNumOutputs() const { return Outputs.Num(); };
	int32 GetNumParameters() const { return Parameters.Num(); };
	int32 GetNumHandles() const { return HandleComponents.Num(); };

	UHoudiniInput* GetInputAt(const int32& Idx) { return Inputs.IsValidIndex(Idx) ? Inputs[Idx] : nullptr; };
	UHoudiniOutput* GetOutputAt(const int32& Idx) { return Outputs.IsValidIndex(Idx) ? Outputs[Idx] : nullptr;};
	UHoudiniParameter* GetParameterAt(const int32& Idx) { return Parameters.IsValidIndex(Idx) ? Parameters[Idx] : nullptr;};
	UHoudiniHandleComponent* GetHandleComponentAt(const int32& Idx) { return HandleComponents.IsValidIndex(Idx) ? HandleComponents[Idx] : nullptr; };

	void GetOutputs(TArray<UHoudiniOutput*>& OutOutputs) const;

	TArray<FHoudiniBakedOutput>& GetBakedOutputs() { return BakedOutputs; }
	const TArray<FHoudiniBakedOutput>& GetBakedOutputs() const { return BakedOutputs; }

	/*
	TArray<UHoudiniParameter*>& GetParameters() { return Parameters; };
	TArray<UHoudiniInput*>& GetInputs() { return Inputs; };
	TArray<UHoudiniOutput*>& GetOutputs() { return Outputs; };
	*/

	bool IsCookingEnabled() const { return bEnableCooking; };
	bool HasBeenLoaded() const { return bHasBeenLoaded; };
	bool HasBeenDuplicated() const { return bHasBeenDuplicated; };
	bool HasRecookBeenRequested() const { return bRecookRequested; };
	bool HasRebuildBeenRequested() const { return bRebuildRequested; };

	//bool GetEditorPropertiesNeedFullUpdate() const { return bEditorPropertiesNeedFullUpdate; };

	int32 GetAssetCookCount() const { return AssetCookCount; };

	bool IsFullyLoaded() const { return bFullyLoaded; };

	UHoudiniPDGAssetLink * GetPDGAssetLink() const { return PDGAssetLink; };

	virtual bool IsProxyStaticMeshEnabled() const;
	bool IsProxyStaticMeshRefinementByTimerEnabled() const;
	float GetProxyMeshAutoRefineTimeoutSeconds() const;
	bool IsProxyStaticMeshRefinementOnPreSaveWorldEnabled() const;
	bool IsProxyStaticMeshRefinementOnPreBeginPIEEnabled() const;
	// If true, then the next cook should not build proxy meshes, regardless of global or override settings,
	// but should instead directly build UStaticMesh
	bool HasNoProxyMeshNextCookBeenRequested() const { return bNoProxyMeshNextCookRequested; }
	// Returns true if the asset state indicates that it has been cooked in this session, false otherwise.
	bool IsHoudiniCookedDataAvailable(bool &bOutNeedsRebuildOrDelete, bool &bOutInvalidState) const;
	// Returns true if the asset should be bake after the next cook
	bool IsBakeAfterNextCookEnabled() const { return BakeAfterNextCook != EHoudiniBakeAfterNextCook::Disabled; }
	// Get the BakeAfterNextCook setting
	EHoudiniBakeAfterNextCook GetBakeAfterNextCook() const { return BakeAfterNextCook; }

	FOnPreInstantiationDelegate& GetOnPreInstantiationDelegate() { return OnPreInstantiationDelegate; }
	FOnPreCookDelegate& GetOnPreCookDelegate() { return OnPreCookDelegate; }
	FOnPostCookDelegate& GetOnPostCookDelegate() { return OnPostCookDelegate; }
	FOnPostBakeDelegate& GetOnPostBakeDelegate() { return OnPostBakeDelegate; }
	FOnPreOutputProcessingDelegate& GetOnPreOutputProcessingDelegate() { return OnPreOutputProcessingDelegate; }
	FOnPostOutputProcessingDelegate& GetOnPostOutputProcessingDelegate() { return OnPostOutputProcessingDelegate; }

	FOnAssetStateChangeDelegate& GetOnAssetStateChangeDelegate() { return OnAssetStateChangeDelegate; }

	// Register a callback that will be fired once during the next PreCook event, after which the callback
	// will be removed from the queue.
	// This is typically used when applying presets during HDA instantiation where we need to wait for
	// the HoudiniAssetComponent to reach it's PreCook phase before we execute the callback to populate the HAC
	// with the desired preset / input values.
	void QueuePreCookCallback(const TFunction<void(UHoudiniAssetComponent*)>& CallbackFn);

	// Derived blueprint based components will check whether the template
	// component contains updates that needs to processed.
	bool NeedUpdateParameters() const;
	bool NeedUpdateInputs() const;

	// Returns true if the component has any previous baked output recorded in its outputs
	bool HasPreviousBakeOutput() const;

	// Returns true if the last cook of the HDA was successful
	bool WasLastCookSuccessful() const { return bLastCookSuccess; }

	// Returns true if a parameter definition update (excluding values) is needed.
	bool IsParameterDefinitionUpdateNeeded() const { return bParameterDefinitionUpdateNeeded; }

	// Returns the BakeFolder, if it is not empty. Otherwise returns the plugin default bake folder. This
	// function does not take the unreal_bake_folder attribute into account.
	FString GetBakeFolderOrDefault() const;

	// Returns the TemporaryCookFolder, if it is not empty. Otherwise returns the plugin default temporary
	// cook folder. This function does not take the unreal_temp_folder attribute into account.
	FString GetTemporaryCookFolderOrDefault() const;

	// Returns true if this asset should try to start a session
	virtual bool ShouldTryToStartFirstSession() const;

	//------------------------------------------------------------------------------------------------
	// Mutators
	//------------------------------------------------------------------------------------------------
	//void SetAssetId(const int& InAssetId);
	//void SetAssetState(const EHoudiniAssetState& InAssetState) { AssetState = InAssetState; };
	//void SetAssetStateResult(const EHoudiniAssetStateResult& InAssetStateResult) { AssetStateResult = InAssetStateResult; };

	//void SetHapiGUID(const FGuid& InGUID) { HapiGUID = InGUID; };
	//void SetComponentGUID(const FGuid& InGUID) { ComponentGUID = InGUID; };

	//UFUNCTION(BlueprintSetter)
	virtual void SetHoudiniAsset(UHoudiniAsset * NewHoudiniAsset);

	void SetCookingEnabled(const bool& bInCookingEnabled) { bEnableCooking = bInCookingEnabled; };
	
	void SetHasBeenLoaded(const bool& InLoaded) { bHasBeenLoaded = InLoaded; };

	void SetHasBeenDuplicated(const bool& InDuplicated) { bHasBeenDuplicated = InDuplicated; };

	//void SetEditorPropertiesNeedFullUpdate(const bool& InUpdate) { bEditorPropertiesNeedFullUpdate = InUpdate; };

	// Marks the assets as needing a recook
	void MarkAsNeedCook();
	// Marks the assets as needing a full rebuild
	void MarkAsNeedRebuild();
	// Marks the asset as needing to be instantiated
	void MarkAsNeedInstantiation();
	// The blueprint has been structurally modified
	void MarkAsBlueprintStructureModified();
	// The blueprint has been modified but not structurally changed.
	void MarkAsBlueprintModified();
	
	//
	void SetAssetCookCount(const int32& InCount) { AssetCookCount = InCount; };
	//
	void SetRecookRequested(const bool& InRecook) { bRecookRequested = InRecook; };
	//
	void SetRebuildRequested(const bool& InRebuild) { bRebuildRequested = InRebuild; };
	//
	void SetHasComponentTransformChanged(const bool& InHasChanged);

	// Set an array of output nodes being tracked.
	// This will remove any cook counts for nodes that are not in this list.
	void SetOutputNodeIds(const TArray<int32>& OutputNodes);
	TArray<int32> GetOutputNodeIds() const { return NodeIdsToCook; }
	TMap<int32, int32> GetOutputNodeCookCounts() const { return OutputNodeCookCounts; }
	
	// Store the latest cook count that was processed for this output node. 
	void SetOutputNodeCookCount(const int& NodeId, const int& CookCount);
	// Compare the current node's cook count against the cached value. If they are different, return true. False otherwise.
	bool HasOutputNodeChanged(const int& NodeId, const int& NewCookCount);
	// Clear output nodes. This will also clear the output node cook counts.
	void ClearOutputNodes();
	// Clear the cook counts for output nodes. This will trigger rebuild of data.
	void ClearOutputNodesCookCount();

	// Set to True to force the next cook to not build a proxy mesh (regardless of global or override settings) and
	// instead build a UStaticMesh directly (if applicable for the output type).
	void SetNoProxyMeshNextCookRequested(bool bInNoProxyMeshNextCookRequested) { bNoProxyMeshNextCookRequested = bInNoProxyMeshNextCookRequested; }

	// Set whether or not bake after cooking (disabled, always or once).
	void SetBakeAfterNextCook(const EHoudiniBakeAfterNextCook InBakeAfterNextCook) { BakeAfterNextCook = InBakeAfterNextCook; }

	//
	void SetPDGAssetLink(UHoudiniPDGAssetLink* InPDGAssetLink);
	//
	virtual void OnHoudiniAssetChanged();

	//
	void AddDownstreamHoudiniAsset(UHoudiniAssetComponent* InDownstreamAsset) { DownstreamHoudiniAssets.Add(InDownstreamAsset); };
	//
	void RemoveDownstreamHoudiniAsset(UHoudiniAssetComponent* InRemoveDownstreamAsset) { DownstreamHoudiniAssets.Remove(InRemoveDownstreamAsset); };
	//
	void ClearDownstreamHoudiniAsset() { DownstreamHoudiniAssets.Empty(); };
	//
	bool NotifyCookedToDownstreamAssets();
	//
	bool NeedsToWaitForInputHoudiniAssets();

	// Clear/disable the RefineMeshesTimer.
	void ClearRefineMeshesTimer();

	// Re-set the RefineMeshesTimer to its default value.
	void SetRefineMeshesTimer();

	virtual void OnRefineMeshesTimerFired();
	
	// Called by RefineMeshesTimer when the timer is triggered.
	// Checks for any UHoudiniStaticMesh in Outputs and bakes UStaticMesh for them via FHoudiniMeshTranslator.	 
	FOnRefineMeshesTimerDelegate& GetOnRefineMeshesTimerDelegate() { return OnRefineMeshesTimerDelegate; }

	// Returns true if the asset is valid for cook/bake
	virtual bool IsComponentValid() const;
	// Return false if this component has no cooking or instantiation in progress.
	bool IsInstantiatingOrCooking() const;

	// HoudiniEngineTick will be called by HoudiniEngineManager::Tick()
	virtual void HoudiniEngineTick();

#if WITH_EDITOR
	// This alternate version of PostEditChange is called when properties inside structs are modified.  The property that was actually modified
	// is located at the tail of the list.  The head of the list of the FStructProperty member variable that contains the property that was modified.
	virtual void PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent) override;

	//Called after applying a transaction to the object.  Default implementation simply calls PostEditChange. 
	virtual void PostEditUndo() override;

	// Whether this component is currently open in a Blueprint editor. This
	// method is overridden by HoudiniAssetBlueprintComponent.
	virtual bool HasOpenEditor() const { return false; };

#endif

	void SetStaticMeshGenerationProperties(UStaticMesh* InStaticMesh) const;

	virtual void RegisterHoudiniComponent(UHoudiniAssetComponent* InComponent);

	virtual void OnRegister() override;

	// USceneComponent methods.
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;

	FBox GetAssetBounds(UHoudiniInput* IgnoreInput, bool bIgnoreGeneratedLandscape) const;

	// return the cached component template, if available.
	virtual UHoudiniAssetComponent* GetCachedTemplate() const { return nullptr; }

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	//------------------------------------------------------------------------------------------------
	// Supported Features
	//------------------------------------------------------------------------------------------------

	// Whether or not this component should be able to delete the Houdini nodes
	// that correspond to the HoudiniAsset when being deregistered. 
	virtual bool CanDeleteHoudiniNodes() const { return true; }

	virtual bool IsInputTypeSupported(EHoudiniInputType InType) const;
	virtual bool IsOutputTypeSupported(EHoudiniOutputType InType) const;

	//------------------------------------------------------------------------------------------------5
	// Characteristics
	//------------------------------------------------------------------------------------------------

	// Try to determine whether this component belongs to a preview actor.
	// Preview / Template components need to sync their data for HDA cooks and output translations.
	bool IsPreview() const;

	virtual bool IsValidComponent() const;

	//------------------------------------------------------------------------------------------------
	// Notifications
	//------------------------------------------------------------------------------------------------

	// TODO: After the cook worfklow rework, most of these won't be needed anymore, so clean up!
	//FHoudiniAssetComponentEvent OnTemplateParametersChanged;
	//FHoudiniAssetComponentEvent OnPreAssetCook;
	//FHoudiniAssetComponentEvent OnCookCompleted;
	//FHoudiniAssetComponentEvent OnOutputProcessingCompleted;

	/*virtual void BroadcastParametersChanged();
	virtual void BroadcastPreAssetCook();
	virtual void BroadcastCookCompleted();*/

	virtual void OnPrePreCook() {};
	virtual void OnPostPreCook() {};
	virtual void OnPreOutputProcessing() { };
	virtual void OnPostOutputProcessing() { };
	virtual void OnPrePreInstantiation() {};


	virtual void NotifyHoudiniRegisterCompleted() {};
	virtual void NotifyHoudiniPreUnregister() {};
	virtual void NotifyHoudiniPostUnregister() {};

	virtual void OnFullyLoaded();

	// Component template parameters have been updated. 
	// Broadcast delegate, and let preview components take care of the rest.
	virtual void OnTemplateParametersChanged() { };
	virtual void OnBlueprintStructureModified() { };
	virtual void OnBlueprintModified() { };

	//
	// Begin: IHoudiniAssetStateEvents
	//

	virtual void HandleOnHoudiniAssetStateChange(UObject* InHoudiniAssetContext, const EHoudiniAssetState InFromState, const EHoudiniAssetState InToState) override;
	
	FORCEINLINE
	virtual FOnHoudiniAssetStateChange& GetOnHoudiniAssetStateChangeDelegate() override { return OnHoudiniAssetStateChangeDelegate; }
	
	//
	// End: IHoudiniAssetStateEvents
	//

	// Called by HandleOnHoudiniAssetStateChange when entering the PostCook state. Broadcasts OnPostCookDelegate.
	void HandleOnPreInstantiation();
	void HandleOnPreCook();
	void HandleOnPostCook();
	void HandleOnPreOutputProcessing();
	void HandleOnPostOutputProcessing();

	// Called by baking code after baking all outputs of this HAC (HoudiniEngineBakeOption)
	void HandleOnPostBake(const bool bInSuccess);

protected:

	// UActorComponents Method
	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	virtual void OnChildAttached(USceneComponent* ChildComponent) override;

	virtual void BeginDestroy() override;

	// 
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;

	// Do any object - specific cleanup required immediately after loading an object.
	// This is not called for newly - created objects, and by default will always execute on the game thread.
	virtual void PostLoad() override;

	// Called after importing property values for this object (paste, duplicate or .t3d import)
	// Allow the object to perform any cleanup for properties which shouldn't be duplicated or
	// Are unsupported by the script serialization
	virtual void PostEditImport() override;
		
	//
	void OnActorMoved(AActor* Actor);

	// 
	void UpdatePostDuplicate();

	//
	//static void AddReferencedObjects(UObject * InThis, FReferenceCollector & Collector);

	// Updates physics state & bounds.
	// Should be call PostLoad and PostProcessing
	void UpdatePhysicsState();

	// Mutators

	// Set asset state
	void SetAssetState(EHoudiniAssetState InNewState);

	void UpdateDormantStatus();

#if (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION > 0)
	ILevelInstanceInterface* GetLevelInstance() const;
#endif

public:


	void OnSessionConnected();

	// Houdini Asset associated with this component.
	/*Category = HoudiniAsset, EditAnywhere, meta = (DisplayPriority=0)*/
	UPROPERTY(Category = HoudiniAsset, EditAnywhere)// BlueprintSetter = SetHoudiniAsset, BlueprintReadWrite, )
	TObjectPtr<UHoudiniAsset> HoudiniAsset;

	// Automatically cook when a parameter or input is changed
	UPROPERTY()
	bool bCookOnParameterChange;

	// Enables uploading of transformation changes back to Houdini Engine.
	UPROPERTY()
	bool bUploadTransformsToHoudiniEngine;

	// Transform changes automatically trigger cooks.
	UPROPERTY()
	bool bCookOnTransformChange;

	// Houdini materials will be converted to Unreal Materials.
	//UPROPERTY()
	//bool bUseNativeHoudiniMaterials;

	// This asset will cook when its asset input cook
	UPROPERTY()
	bool bCookOnAssetInputCook;

	// Enabling this will prevent the HDA from producing any output after cooking.
	UPROPERTY()
	bool bOutputless;

	// Enabling this will allow outputing the asset's templated geos
	UPROPERTY()
	bool bOutputTemplateGeos;

	// Enabling this will allow outputing the asset's output nodes
	UPROPERTY()
	bool bUseOutputNodes;


	// Temporary cook folder
	UPROPERTY()
	FDirectoryPath TemporaryCookFolder;

	// Folder used for baking this asset's outputs (unless set by prim/detail attribute on the output). Falls back to
	// the default from the plugin settings if not set.
	UPROPERTY()
	FDirectoryPath BakeFolder;
	
	// Whether or not to support multiple mesh outputs on one HDA output. This is currently in Alpha  testing.
	UPROPERTY(Category = "HoudiniMeshGeneration", EditAnywhere, meta = (DisplayPriority = 0))
	bool bSplitMeshSupport = false;

	// Generation properties for the Static Meshes generated by this Houdini Asset
	UPROPERTY(Category = "HoudiniMeshGeneration", EditAnywhere, meta = (DisplayPriority = 1)/*, meta = (ShowOnlyInnerProperties)*/)
	FHoudiniStaticMeshGenerationProperties StaticMeshGenerationProperties;

	// Build Settings to be used when generating the Static Meshes for this Houdini Asset
	UPROPERTY(Category = "HoudiniMeshGeneration", EditAnywhere, meta = (DisplayPriority = 2))
	FMeshBuildSettings StaticMeshBuildSettings;

	// Override the global fast proxy mesh settings on this component?
	UPROPERTY(Category = "HoudiniProxyMeshGeneration", EditAnywhere/*, meta = (DisplayAfter = "StaticMeshGenerationProperties")*/)
	bool bOverrideGlobalProxyStaticMeshSettings;

	// For StaticMesh outputs: should a fast proxy be created first?
	UPROPERTY(Category = "HoudiniProxyMeshGeneration", EditAnywhere, meta = (DisplayName="Enable Proxy Static Mesh", EditCondition="bOverrideGlobalProxyStaticMeshSettings"))
	bool bEnableProxyStaticMeshOverride;

	// If fast proxy meshes are being created, must it be baked as a StaticMesh after a period of no updates?
	UPROPERTY(Category = "HoudiniProxyMeshGeneration", EditAnywhere, meta = (DisplayName="Refine Proxy Static Meshes After a Timeout", EditCondition = "bOverrideGlobalProxyStaticMeshSettings && bEnableProxyStaticMeshOverride"))
	bool bEnableProxyStaticMeshRefinementByTimerOverride;
	
	// If the option to automatically refine the proxy mesh via a timer has been selected, this controls the timeout in seconds.
	UPROPERTY(Category = "HoudiniProxyMeshGeneration", EditAnywhere, meta = (DisplayName="Proxy Mesh Auto Refine Timeout Seconds", EditCondition = "bOverrideGlobalProxyStaticMeshSettings && bEnableProxyStaticMeshOverride && bEnableProxyStaticMeshRefinementByTimerOverride"))
	float ProxyMeshAutoRefineTimeoutSecondsOverride;

	// Automatically refine proxy meshes to UStaticMesh before the map is saved
	UPROPERTY(Category = "HoudiniProxyMeshGeneration", EditAnywhere, meta = (DisplayName="Refine Proxy Static Meshes When Saving a Map", EditCondition = "bOverrideGlobalProxyStaticMeshSettings && bEnableProxyStaticMeshOverride"))
	bool bEnableProxyStaticMeshRefinementOnPreSaveWorldOverride;

	// Automatically refine proxy meshes to UStaticMesh before starting a play in editor session
	UPROPERTY(Category = "HoudiniProxyMeshGeneration", EditAnywhere, meta = (DisplayName="Refine Proxy Static Meshes On PIE", EditCondition = "bOverrideGlobalProxyStaticMeshSettings && bEnableProxyStaticMeshOverride"))
	bool bEnableProxyStaticMeshRefinementOnPreBeginPIEOverride;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bGenerateMenuExpanded;

	UPROPERTY()
	bool bBakeMenuExpanded;

	UPROPERTY()
	bool bAssetOptionMenuExpanded;

	UPROPERTY()
	bool bHelpAndDebugMenuExpanded;

	UPROPERTY()
	EHoudiniEngineBakeOption HoudiniEngineBakeOption;

	// If true, then after a successful bake, the HACs outputs will be cleared and removed.
	UPROPERTY()
	bool bRemoveOutputAfterBake;

	// If true, recenter baked actors to their bounding box center after bake
	UPROPERTY()
	bool bRecenterBakedActors;

	// If true, replace the previously baked output (if any) instead of creating new objects
	UPROPERTY()
	bool bReplacePreviousBake;

	UPROPERTY()
	EHoudiniEngineActorBakeOption ActorBakeOption;

	UPROPERTY()
	bool bLandscapeUseTempLayers;

	UPROPERTY()
	bool bEnableCurveEditing;

	// Indicates whether or not this component should update the editor's UI
	// This is to prevent successive calls of the function for the same HDAs 
	UPROPERTY(Transient, DuplicateTransient)
	bool bNeedToUpdateEditorProperties;
#endif

protected:

	// Id of corresponding Houdini asset.
	UPROPERTY(DuplicateTransient)
	int32 AssetId;

	// Ids of the nodes that should be cook for this HAC
	// This is for additional output and templated nodes if they are used.
	UPROPERTY(Transient, DuplicateTransient)
	TArray<int32> NodeIdsToCook;

	// Cook counts for nodes in the NodeIdsToCook array.
	UPROPERTY(Transient, DuplicateTransient)
	TMap<int32, int32> OutputNodeCookCounts;

	// List of dependent downstream HACs that have us as an asset input
	UPROPERTY(DuplicateTransient)
	TSet<TObjectPtr<UHoudiniAssetComponent>> DownstreamHoudiniAssets;

	// Unique GUID created by component.
	UPROPERTY(DuplicateTransient)
	FGuid ComponentGUID;

	// GUID used to track asynchronous cooking requests.
	UPROPERTY(DuplicateTransient)
	FGuid HapiGUID;

	// The asset name of the selected asset inside the asset library
	UPROPERTY(DuplicateTransient)
	FString HapiAssetName;

	// Current state of the asset
	UPROPERTY(DuplicateTransient)
	EHoudiniAssetState AssetState;

	// Last asset state logged.
	UPROPERTY(DuplicateTransient)
	mutable EHoudiniAssetState DebugLastAssetState;

	// Result of the current asset's state
	UPROPERTY(DuplicateTransient)
	EHoudiniAssetStateResult AssetStateResult;

	// Used to compare transform changes and whether we need to
	// send transform updates to Houdini.
	UPROPERTY(DuplicateTransient)
	FTransform LastComponentTransform;

	//// Contains the context for keeping track of shared 
	//// Houdini data.
	//UPROPERTY(DuplicateTransient)
	//UHoudiniAssetContext* AssetContext;

	// Subasset index
	UPROPERTY()
	uint32 SubAssetIndex;
	
	// Number of times this asset has been cooked.
	UPROPERTY(DuplicateTransient)
	int32 AssetCookCount;

	// 
	UPROPERTY(DuplicateTransient)
	bool bHasBeenLoaded;

	// Sometimes, specifically when editing level instances, the Unreal Editor will duplicate the HDA,
	// then duplicate it again, before we get a change to call UpdatePostDuplicate().
	// So bHasBeenDuplicated should not be cleared and is so not marked DuplicateTransient.
	UPROPERTY()
	bool bHasBeenDuplicated;

	UPROPERTY(DuplicateTransient)
	bool bPendingDelete;

	UPROPERTY(DuplicateTransient)
	bool bRecookRequested;

	UPROPERTY(DuplicateTransient)
	bool bRebuildRequested;

	UPROPERTY(DuplicateTransient)
	bool bEnableCooking;

	UPROPERTY(DuplicateTransient)
	bool bForceNeedUpdate;

	UPROPERTY(DuplicateTransient)
	bool bLastCookSuccess;

	// Indicates that the parameter state (excluding values) on the HAC and the instantiated node needs to be synced.
	// The most common use for this would be a newly instantiated HDA that has only a default parameter interface
	// from its asset definition, and needs to sync pre-cook.
	UPROPERTY(DuplicateTransient)
	bool bParameterDefinitionUpdateNeeded;

	UPROPERTY(DuplicateTransient)
	bool bBlueprintStructureModified;

	UPROPERTY(DuplicateTransient)
	bool bBlueprintModified;
	
	//UPROPERTY(DuplicateTransient)
	//bool bEditorPropertiesNeedFullUpdate;

	UPROPERTY(Instanced)
	TArray<TObjectPtr<UHoudiniParameter>> Parameters;

	UPROPERTY(Instanced)
	TArray<TObjectPtr<UHoudiniInput>> Inputs;
	
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UHoudiniOutput>> Outputs;

	// The baked outputs from the last bake.
	UPROPERTY()
	TArray<FHoudiniBakedOutput> BakedOutputs;

	// Any actors that aren't explicitly
	// tracked by output objects should be registered
	// here so that they can be cleaned up.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> UntrackedOutputs;

	UPROPERTY()
	TArray<TObjectPtr<UHoudiniHandleComponent>> HandleComponents;

	UPROPERTY(Transient, DuplicateTransient)
	bool bHasComponentTransformChanged;

	UPROPERTY(Transient, DuplicateTransient)
	bool bFullyLoaded;

	UPROPERTY()
	TObjectPtr<UHoudiniPDGAssetLink> PDGAssetLink;

	UPROPERTY()
	bool bIsPDGAssetLinkInitialized;

	// Timer that is used to trigger creation of UStaticMesh for all mesh outputs
	// that still have UHoudiniStaticMeshes. The timer is cleared on PreCook and reset
	// at the end of the PostCook.
	UPROPERTY()
	FTimerHandle RefineMeshesTimer;

	// Delegate that is used to broadcast when RefineMeshesTimer fires
	FOnRefineMeshesTimerDelegate OnRefineMeshesTimerDelegate;

	// If true, don't build a proxy mesh next cook (regardless of global or override settings),
	// instead build the UStaticMesh directly (if applicable for the output types).
	UPROPERTY(DuplicateTransient)
	bool bNoProxyMeshNextCookRequested;
	
	// If true, bake the asset after its next cook.
	UPROPERTY(DuplicateTransient)
	EHoudiniBakeAfterNextCook BakeAfterNextCook;

	// Delegate to broadcast before instantiation
	// Arguments are (HoudiniAssetComponent* HAC)
	FOnPreInstantiationDelegate OnPreInstantiationDelegate;

	// Delegate to broadcast after a post cook event
	// Arguments are (HoudiniAssetComponent* HAC, bool IsSuccessful)
	FOnPreCookDelegate OnPreCookDelegate;

	// Delegate to broadcast after a post cook event
	// Arguments are (HoudiniAssetComponent* HAC, bool IsSuccessful)
	FOnPostCookDelegate OnPostCookDelegate;

	// Delegate to broadcast after baking the HAC. Not called when just baking individual outputs directly.
	// Arguments are (HoudiniAssetComponent* HAC, bool bIsSuccessful)
	FOnPostBakeDelegate OnPostBakeDelegate;

	FOnPostOutputProcessingDelegate OnPostOutputProcessingDelegate;
	FOnPreOutputProcessingDelegate OnPreOutputProcessingDelegate;

	// Delegate that is broadcast when the asset state changes (HAC version).
	FOnAssetStateChangeDelegate OnAssetStateChangeDelegate;

	// Cached flag of whether this object is considered to be a 'preview' component or not.
	// This is typically useful in destructors when references to the World, for example, 
	// is no longer available.
	UPROPERTY(Transient, DuplicateTransient)
	bool bCachedIsPreview;

	// The last timestamp this component was ticked
	// used to prioritize/limit the number of HAC processed per tick
	UPROPERTY(Transient)
	double LastTickTime;

	// The last timestamp this component received a session sync update ping
	// used to limit the frequency at which we ping HDAs for session sync updates
	UPROPERTY(Transient)
	double LastLiveSyncPingTime;

	UPROPERTY()
	TArray<int8> ParameterPresetBuffer;

	//
	// Begin: IHoudiniAssetStateEvents
	//

	// Delegate that is broadcast when AssetState changes
	FOnHoudiniAssetStateChange OnHoudiniAssetStateChangeDelegate;

	//
	// End: IHoudiniAssetStateEvents
	//

	// Store any PreCookCallbacks here until they HAC is ready to process them during the PreCook event.
	TArray< TFunction<void(UHoudiniAssetComponent*)> > PreCookCallbacks;
	
#if WITH_EDITORONLY_DATA

public:
	// Sets whether this HDA is allowed to be cooked in PIE
	// for the purposes of refinement.
	void SetAllowPlayInEditorRefinement(bool bEnabled) { bAllowPlayInEditorRefinement = bEnabled; }
	bool IsPlayInEditorRefinementAllowed() const { return bAllowPlayInEditorRefinement; }

protected:
	UPROPERTY(Transient, DuplicateTransient)
	bool bAllowPlayInEditorRefinement;

#endif
	
};
