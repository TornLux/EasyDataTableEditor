#include "EasyCompositeDataTableEditor.h"
#include "EasyDataTableEditorModule.h"
#include "Delegates/Delegate.h"
#include "DetailsViewArgs.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/CompositeDataTable.h"
#include "Engine/DataTable.h"
#include "Framework/Docking/TabManager.h"
#include "IDetailsView.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SEasyCompositeRowEditor.h"
#include "Styling/AppStyle.h"
#include "Templates/Casts.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"

class SRowEditor;
class SWidget;
 
#define LOCTEXT_NAMESPACE "CompositeDataTableEditor"

const FName FEasyCompositeDataTableEditor::PropertiesTabId("CompositeDataTableEditor_Properties");
const FName FEasyCompositeDataTableEditor::StackTabId("CompositeDataTableEditor_Stack");


FEasyCompositeDataTableEditor::FEasyCompositeDataTableEditor()
{
}

FEasyCompositeDataTableEditor::~FEasyCompositeDataTableEditor()
{
}

void FEasyCompositeDataTableEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FEasyDataTableEditor::RegisterTabSpawners(InTabManager);

	CreateAndRegisterPropertiesTab(InTabManager);
}

void FEasyCompositeDataTableEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FEasyDataTableEditor::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(PropertiesTabId);
	InTabManager->UnregisterTabSpawner(StackTabId);

	DetailsView.Reset();
	StackTabWidget.Reset();
}

void FEasyCompositeDataTableEditor::CreateAndRegisterRowEditorTab(const TSharedRef<class FTabManager>& InTabManager)
{
	// no row editor in the composite data tables
	RowEditorTabWidget.Reset();
}

void FEasyCompositeDataTableEditor::CreateAndRegisterPropertiesTab(const TSharedRef<class FTabManager>& InTabManager)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	InTabManager->RegisterTabSpawner(PropertiesTabId, FOnSpawnTab::CreateSP(this, &FEasyCompositeDataTableEditor::SpawnTab_Properties))
		.SetDisplayName(LOCTEXT("PropertiesTab", "Properties"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FEasyCompositeDataTableEditor::InitDataTableEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UDataTable* Table)
{
	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_CompositeDataTableEditor_temp_Layout_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.3f)	
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->SetHideTabWell(true)
					->AddTab(PropertiesTabId, ETabState::OpenedTab)
				)
// 				->Split
// 				(
// 					FTabManager::NewStack()		
// 					->SetHideTabWell(true)
// 					->AddTab(StackTabId, ETabState::OpenedTab)
// 				)

			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(DataTableTabId, ETabState::OpenedTab)
				)
// 				->Split
// 				(
// 					FTabManager::NewStack()
// 					->AddTab(RowEditorTabId, ETabState::OpenedTab)
// 				)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FEasyDataTableEditorModule::DataTableEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Table);

	FEasyDataTableEditorModule& DataTableEditorModule = FModuleManager::LoadModuleChecked<FEasyDataTableEditorModule>("DataTableEditor");
	AddMenuExtender(DataTableEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	// Support undo/redo
	GEditor->RegisterForUndo(this);

	if (DetailsView.IsValid())
	{
		// Make sure details window is pointing to our object
		DetailsView->SetObject(GetEditingObject());
	}
}

bool FEasyCompositeDataTableEditor::CanEditRows() const
{
	return false;
}

TSharedRef<SDockTab> FEasyCompositeDataTableEditor::SpawnTab_Stack(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == StackTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("StackTitle", "Datatable Stack"))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SBorder)
			.Padding(2)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				StackTabWidget.ToSharedRef()
			]
		];
}

TSharedRef<SDockTab> FEasyCompositeDataTableEditor::SpawnTab_Properties(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == PropertiesTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("PropertiesTitle", "Properties"))
		.TabColorScale(GetTabColorScale())
		[
			DetailsView.ToSharedRef()
		];
}

TSharedRef<SWidget> FEasyCompositeDataTableEditor::CreateStackBox()
{
	UDataTable* Table = Cast<UDataTable>(GetEditingObject());

	// Support undo/redo
	if (Table)
	{
		Table->SetFlags(RF_Transactional);
	}

	return CreateRowEditor(Table);
}

TSharedRef<SEasyRowEditor> FEasyCompositeDataTableEditor::CreateRowEditor(UDataTable* Table)
{
	UCompositeDataTable* DataTable = Cast<UCompositeDataTable>(Table);
	return SNew(SEasyCompositeRowEditor, DataTable);

}

#undef LOCTEXT_NAMESPACE