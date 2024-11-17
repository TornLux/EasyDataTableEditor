// Fill out your copyright notice in the Description page of Project Settings.


#include "EasyDataTableEditor.h"

#include "AssetRegistry/AssetData.h"
#include "Containers/Map.h"
#include "CoreGlobals.h"
#include "EasyDataTableEditorModule.h"
#include "DataTableUtils.h"
#include "DetailsViewArgs.h"
#include "EasyDataTableEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/DataTable.h"
#include "Engine/UserDefinedStruct.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Layout/Overscroll.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Framework/Text/TextLayout.h"
#include "Framework/Views/ITypedTableView.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IDetailsView.h"
#include "IDocumentation.h"
#include "Internationalization/Internationalization.h"
#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Math/ColorList.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "PropertyEditorModule.h"
#include "Rendering/SlateRenderer.h"
#include "SEasyDataTableListViewRow.h"
#include "SEasyRowEditor.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SlotBase.h"
#include "SourceCodeNavigation.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Casts.h"
#include "Templates/TypeHash.h"
#include "Textures/SlateIcon.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/Class.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UObjectBaseUtility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class STableViewBase;
class SWidget;

#define LOCTEXT_NAMESPACE "EasyDataTableEditor"

const FName FEasyDataTableEditor::DataTableTabId("DataTableEditor_DataTable");
const FName FEasyDataTableEditor::DataTableDetailsTabId("DataTableEditor_DataTableDetails");
const FName FEasyDataTableEditor::RowEditorTabId("DataTableEditor_RowEditor");
const FName FEasyDataTableEditor::RowNameColumnId("RowName");
const FName FEasyDataTableEditor::RowNumberColumnId("RowNumber");
const FName FEasyDataTableEditor::RowDragDropColumnId("RowDragDrop");

class SDataTableModeSeparator : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SDataTableModeSeparator) {}
	SLATE_END_ARGS()

		void Construct(const FArguments& InArg)
	{
		SBorder::Construct(
			SBorder::FArguments()
			.BorderImage(FAppStyle::GetBrush("BlueprintEditor.PipelineSeparator"))
			.Padding(0.0f)
		);
	}

	// SWidget interface
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		const float Height = 20.0f;
		const float Thickness = 16.0f;
		return FVector2D(Thickness, Height);
	}
	// End of SWidget interface
};


void FEasyDataTableEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_Data Table Editor", "Data Table Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	CreateAndRegisterDataTableTab(InTabManager);
	CreateAndRegisterDataTableDetailsTab(InTabManager);
	CreateAndRegisterRowEditorTab(InTabManager);
}

void FEasyDataTableEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(DataTableTabId);
	InTabManager->UnregisterTabSpawner(DataTableDetailsTabId);
	InTabManager->UnregisterTabSpawner(RowEditorTabId);

	DataTableTabWidget.Reset();
	RowEditorTabWidget.Reset();
}

void FEasyDataTableEditor::CreateAndRegisterDataTableTab(const TSharedRef<class FTabManager>& InTabManager)
{
	DataTableTabWidget = CreateContentBox();

	InTabManager->RegisterTabSpawner(DataTableTabId, FOnSpawnTab::CreateSP(this, &FEasyDataTableEditor::SpawnTab_DataTable))
		.SetDisplayName(LOCTEXT("DataTableTab", "Data Table"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FEasyDataTableEditor::CreateAndRegisterDataTableDetailsTab(const TSharedRef<class FTabManager>& InTabManager)
{
	FPropertyEditorModule & EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	PropertyView = EditModule.CreateDetailView(DetailsViewArgs);

	InTabManager->RegisterTabSpawner(DataTableDetailsTabId, FOnSpawnTab::CreateSP(this, &FEasyDataTableEditor::SpawnTab_DataTableDetails))
		.SetDisplayName(LOCTEXT("DataTableDetailsTab", "Data Table Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FEasyDataTableEditor::CreateAndRegisterRowEditorTab(const TSharedRef<class FTabManager>& InTabManager)
{
	RowEditorTabWidget = CreateRowEditorBox();

	InTabManager->RegisterTabSpawner(RowEditorTabId, FOnSpawnTab::CreateSP(this, &FEasyDataTableEditor::SpawnTab_RowEditor))
		.SetDisplayName(LOCTEXT("RowEditorTab", "Row Editor"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

FEasyDataTableEditor::FEasyDataTableEditor()
	: RowNameColumnWidth(0)
	, RowNumberColumnWidth(0)
	, HighlightedVisibleRowIndex(INDEX_NONE)
	, SortMode(EColumnSortMode::Ascending)
{
}

FEasyDataTableEditor::~FEasyDataTableEditor()
{
	GEditor->UnregisterForUndo(this);

	UDataTable* Table = GetEditableDataTable();
	if (Table)
	{
		SaveLayoutData();
	}
}

void FEasyDataTableEditor::PostUndo(bool bSuccess)
{
	HandleUndoRedo();
}

void FEasyDataTableEditor::PostRedo(bool bSuccess)
{
	HandleUndoRedo();
}

void FEasyDataTableEditor::HandleUndoRedo()
{
	const UDataTable* Table = GetDataTable();
	if (Table)
	{
		HandlePostChange();
		CallbackOnDataTableUndoRedo.ExecuteIfBound();
	}
}

void FEasyDataTableEditor::PreChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
}

void FEasyDataTableEditor::PostChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
	const UDataTable* Table = GetDataTable();
	if (Struct && Table && (Table->GetRowStruct() == Struct))
	{
		HandlePostChange();
	}
}

void FEasyDataTableEditor::SelectionChange(const UDataTable* Changed, FName RowName)
{
	const UDataTable* Table = GetDataTable();
	if (Changed == Table)
	{
		const bool bSelectionChanged = HighlightedRowName != RowName;
		SetHighlightedRow(RowName);

		if (bSelectionChanged)
		{
			CallbackOnRowHighlighted.ExecuteIfBound(HighlightedRowName);
		}
	}
}

void FEasyDataTableEditor::PreChange(const UDataTable* Changed, FEasyDataTableEditorUtils::EDataTableChangeInfo Info)
{
	/*
	UE_LOG(LogTemp,Log,TEXT("HeightLight:%s"),*HighlightedRowName.ToString());
	for(auto& Iter:CellsListView->GetSelectedItems())
	{
		UE_LOG(LogTemp,Log,TEXT("Item:%s"),*Iter->RowId.ToString());
	}*/
}

void FEasyDataTableEditor::PostChange(const UDataTable* Changed, FEasyDataTableEditorUtils::EDataTableChangeInfo Info)
{
	UDataTable* Table = GetEditableDataTable();
	if (Changed == Table)
	{
		// Don't need to notify the DataTable about changes, that's handled before this
		HandlePostChange();
	}
}

const UDataTable* FEasyDataTableEditor::GetDataTable() const
{
	return Cast<const UDataTable>(GetEditingObject());
}

void FEasyDataTableEditor::HandlePostChange()
{
	// We need to cache and restore the selection here as RefreshCachedDataTable will re-create the list view items
	const FName CachedSelection = HighlightedRowName;
	HighlightedRowName = NAME_None;
	RefreshCachedDataTable(CachedSelection, true/*bUpdateEvenIfValid*/);
}

void FEasyDataTableEditor::InitDataTableEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UDataTable* Table )
{
	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_DataTableEditor_Layout_v6" )
	->AddArea
	(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->AddTab(DataTableTabId, ETabState::OpenedTab)
			->AddTab(DataTableDetailsTabId, ETabState::OpenedTab)
			->SetForegroundTab(DataTableTabId)
		)
		->Split
		(
			FTabManager::NewStack()
			->AddTab(RowEditorTabId, ETabState::OpenedTab)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FEasyDataTableEditorModule::DataTableEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Table );

	FEasyDataTableEditorModule& DataTableEditorModule = FModuleManager::LoadModuleChecked<FEasyDataTableEditorModule>( "DataTableEditor" );
	AddMenuExtender(DataTableEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
	
	TSharedPtr<FExtender> ToolbarExtender = DataTableEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects());
	ExtendToolbar(ToolbarExtender);
	
	AddToolbarExtender(ToolbarExtender);

	RegenerateMenusAndToolbars();

	// Support undo/redo
	GEditor->RegisterForUndo(this);

	// @todo toolkit world centric editing
	/*// Setup our tool's layout
	if( IsWorldCentricAssetEditor() )
	{
		const FString TabInitializationPayload(TEXT(""));		// NOTE: Payload not currently used for table properties
		SpawnToolkitTab( DataTableTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/

	// asset editor commands here
	ToolkitCommands->MapAction(FGenericCommands::Get().Copy, FExecuteAction::CreateSP(this, &FEasyDataTableEditor::CopySelectedRow), FCanExecuteAction::CreateSP(this, &FEasyDataTableEditor::CanEditTable));
	ToolkitCommands->MapAction(FGenericCommands::Get().Paste, FExecuteAction::CreateSP(this, &FEasyDataTableEditor::PasteOnSelectedRow), FCanExecuteAction::CreateSP(this, &FEasyDataTableEditor::CanEditTable));
	ToolkitCommands->MapAction(FGenericCommands::Get().Duplicate, FExecuteAction::CreateSP(this, &FEasyDataTableEditor::DuplicateSelectedRow), FCanExecuteAction::CreateSP(this, &FEasyDataTableEditor::CanEditTable));
	ToolkitCommands->MapAction(FGenericCommands::Get().Rename, FExecuteAction::CreateSP(this, &FEasyDataTableEditor::RenameSelectedRowCommand), FCanExecuteAction::CreateSP(this, &FEasyDataTableEditor::CanEditTable));
	ToolkitCommands->MapAction(FGenericCommands::Get().Delete, FExecuteAction::CreateSP(this, &FEasyDataTableEditor::DeleteSelectedRow), FCanExecuteAction::CreateSP(this, &FEasyDataTableEditor::CanEditTable));
}

bool FEasyDataTableEditor::CanEditRows() const
{
	return true;
}

FName FEasyDataTableEditor::GetToolkitFName() const
{
	return FName("DataTableEditor");
}

FString FEasyDataTableEditor::GetDocumentationLink() const
{
	return FString(TEXT("Gameplay/DataDriven"));
}

void FEasyDataTableEditor::OnAddClicked()
{
	UDataTable* Table = GetEditableDataTable();

	if (Table)
	{		
		FName NewName = DataTableUtils::MakeValidName(TEXT("NewRow"));
		while (Table->GetRowMap().Contains(NewName))
		{
			NewName.SetNumber(NewName.GetNumber() + 1);
		}

		FEasyDataTableEditorUtils::AddRow(Table, NewName);
		FEasyDataTableEditorUtils::SelectRow(Table, NewName);

		SetDefaultSort();
	}
}

void FEasyDataTableEditor::OnRemoveClicked()
{
	DeleteSelectedRow();
}

FReply FEasyDataTableEditor::OnMoveRowClicked(FEasyDataTableEditorUtils::ERowMoveDirection MoveDirection)
{
	UDataTable* Table = GetEditableDataTable();

	if (Table)
	{
		FEasyDataTableEditorUtils::MoveRow(Table, HighlightedRowName, MoveDirection);
	}
	return FReply::Handled();
}

FReply FEasyDataTableEditor::OnMoveToExtentClicked(FEasyDataTableEditorUtils::ERowMoveDirection MoveDirection)
{
	UDataTable* Table = GetEditableDataTable();

	if (Table)
	{
		// We move by the row map size, as FEasyDataTableEditorUtils::MoveRow will automatically clamp this as appropriate
		FEasyDataTableEditorUtils::MoveRow(Table, HighlightedRowName, MoveDirection, Table->GetRowMap().Num());
		FEasyDataTableEditorUtils::SelectRow(Table, HighlightedRowName);

		SetDefaultSort();
	}
	return FReply::Handled();
}

void FEasyDataTableEditor::OnCopyClicked()
{
	UDataTable* Table = GetEditableDataTable();
	if (Table)
	{
		CopySelectedRow();
	}
}

void FEasyDataTableEditor::OnPasteClicked()
{
	UDataTable* Table = GetEditableDataTable();
	if (Table)
	{
		PasteOnSelectedRow();
	}
}

void FEasyDataTableEditor::OnDuplicateClicked()
{
	UDataTable* Table = GetEditableDataTable();
	if (Table)
	{
		DuplicateSelectedRow();
	}
}

bool FEasyDataTableEditor::CanEditTable() const
{
	return HighlightedRowName != NAME_None;
}

void FEasyDataTableEditor::SetDefaultSort()
{
	SortMode = EColumnSortMode::Ascending;
	SortByColumn = FEasyDataTableEditor::RowNumberColumnId;
}

EColumnSortMode::Type FEasyDataTableEditor::GetColumnSortMode(const FName ColumnId) const
{
	if (SortByColumn != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return SortMode;
}

void FEasyDataTableEditor::OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode)
{
	int32 ColumnIndex;

	SortMode = InSortMode;
	SortByColumn = ColumnId;

	for (ColumnIndex = 0; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
	{
		if (AvailableColumns[ColumnIndex]->ColumnId == ColumnId)
		{
			break;
		}
	}

	if (AvailableColumns.IsValidIndex(ColumnIndex))
	{
		if (InSortMode == EColumnSortMode::Ascending)
		{
			VisibleRows.Sort([ColumnIndex](const FEasyDataTableEditorRowListViewDataPtr& first, const FEasyDataTableEditorRowListViewDataPtr& second)
			{					
				int32 Result = (first->CellData[ColumnIndex].ToString()).Compare(second->CellData[ColumnIndex].ToString());

				if (!Result)
				{
					return first->RowNum < second->RowNum;

				}

				return Result < 0;
			});

		}
		else if (InSortMode == EColumnSortMode::Descending)
		{
			VisibleRows.Sort([ColumnIndex](const FEasyDataTableEditorRowListViewDataPtr& first, const FEasyDataTableEditorRowListViewDataPtr& second)
			{
				int32 Result = (first->CellData[ColumnIndex].ToString()).Compare(second->CellData[ColumnIndex].ToString());

				if (!Result)
				{
					return first->RowNum > second->RowNum;
				}

				return Result > 0;
			});
		}
	}

	CellsListView->RequestListRefresh();
}

void FEasyDataTableEditor::OnColumnNumberSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode)
{
	SortMode = InSortMode;
	SortByColumn = ColumnId;

	if (InSortMode == EColumnSortMode::Ascending)
	{
		VisibleRows.Sort([](const FEasyDataTableEditorRowListViewDataPtr& first, const FEasyDataTableEditorRowListViewDataPtr& second)
		{
			return first->RowNum < second->RowNum;
		});
	}
	else if (InSortMode == EColumnSortMode::Descending)
	{
		VisibleRows.Sort([](const FEasyDataTableEditorRowListViewDataPtr& first, const FEasyDataTableEditorRowListViewDataPtr& second)
		{
			return first->RowNum > second->RowNum;
		});
	}

	CellsListView->RequestListRefresh();
}

void FEasyDataTableEditor::OnColumnNameSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode)
{
	SortMode = InSortMode;
	SortByColumn = ColumnId;

	if (InSortMode == EColumnSortMode::Ascending)
	{
		VisibleRows.Sort([](const FEasyDataTableEditorRowListViewDataPtr& first, const FEasyDataTableEditorRowListViewDataPtr& second)
		{
			return (first->DisplayName).ToString() < (second->DisplayName).ToString();
		});
	}
	else if (InSortMode == EColumnSortMode::Descending)
	{
		VisibleRows.Sort([](const FEasyDataTableEditorRowListViewDataPtr& first, const FEasyDataTableEditorRowListViewDataPtr& second)
		{
			return (first->DisplayName).ToString() > (second->DisplayName).ToString();
		});
	}

	CellsListView->RequestListRefresh();
}

void FEasyDataTableEditor::OnEditDataTableStructClicked()
{
	const UDataTable* DataTable = GetDataTable();
	if (DataTable)
	{
		const UScriptStruct* ScriptStruct = DataTable->GetRowStruct();

		if (ScriptStruct)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(ScriptStruct->GetPathName());
			if (FSourceCodeNavigation::CanNavigateToStruct(ScriptStruct))
			{
				FSourceCodeNavigation::NavigateToStruct(ScriptStruct);
			}
		}
	}
}

void FEasyDataTableEditor::ExtendToolbar(TSharedPtr<FExtender> Extender)
{
	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FEasyDataTableEditor::FillToolbar)
	);

}

void FEasyDataTableEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("DataTableCommands");
	{
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FEasyDataTableEditor::Reimport_Execute),
				FCanExecuteAction::CreateSP(this, &FEasyDataTableEditor::CanReimport)),
			NAME_None,
			LOCTEXT("ReimportText", "Reimport"),
			LOCTEXT("ReimportTooltip", "Reimport this DataTable"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"));

		ToolbarBuilder.AddSeparator();

		ToolbarBuilder.AddToolBarButton(
			FUIAction(FExecuteAction::CreateSP(this, &FEasyDataTableEditor::OnAddClicked)),
			NAME_None,
			LOCTEXT("AddIconText", "Add"),
			LOCTEXT("AddRowToolTip", "Add a new row to the Data Table"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"));
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FEasyDataTableEditor::OnCopyClicked),
				FCanExecuteAction::CreateSP(this, &FEasyDataTableEditor::CanEditTable)),
			NAME_None,
			LOCTEXT("CopyIconText", "Copy"),
			LOCTEXT("CopyToolTip", "Copy the currently selected row"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"));
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FEasyDataTableEditor::OnPasteClicked),
				FCanExecuteAction::CreateSP(this, &FEasyDataTableEditor::CanEditTable)),
			NAME_None,
			LOCTEXT("PasteIconText", "Paste"),
			LOCTEXT("PasteToolTip", "Paste on the currently selected row"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Paste"));
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FEasyDataTableEditor::OnDuplicateClicked),
				FCanExecuteAction::CreateSP(this, &FEasyDataTableEditor::CanEditTable)),
			NAME_None,
			LOCTEXT("DuplicateIconText", "Duplicate"),
			LOCTEXT("DuplicateToolTip", "Duplicate the currently selected row"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Duplicate"));
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FEasyDataTableEditor::OnRemoveClicked),
				FCanExecuteAction::CreateSP(this, &FEasyDataTableEditor::CanEditTable)),
			NAME_None,
			LOCTEXT("RemoveRowIconText", "Remove"),
			LOCTEXT("RemoveRowToolTip", "Remove the currently selected row from the Data Table"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"));
	}
	ToolbarBuilder.EndSection();

}

UDataTable* FEasyDataTableEditor::GetEditableDataTable() const
{
	return Cast<UDataTable>(GetEditingObject());
}

FText FEasyDataTableEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "DataTable Editor" );
}

FString FEasyDataTableEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "DataTable ").ToString();
}

FLinearColor FEasyDataTableEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

FSlateColor FEasyDataTableEditor::GetRowTextColor(FName RowName) const
{
	if (RowName == HighlightedRowName)
	{
		return FSlateColor(FColorList::Orange);
	}
	return FSlateColor::UseForeground();
}

FText FEasyDataTableEditor::GetCellText(FEasyDataTableEditorRowListViewDataPtr InRowDataPointer, int32 ColumnIndex) const
{
	if (InRowDataPointer.IsValid() && ColumnIndex < InRowDataPointer->CellData.Num())
	{
		return InRowDataPointer->CellData[ColumnIndex];
	}

	return FText();
}

FText FEasyDataTableEditor::GetCellToolTipText(FEasyDataTableEditorRowListViewDataPtr InRowDataPointer, int32 ColumnIndex) const
{
	FText TooltipText;

	if (ColumnIndex < AvailableColumns.Num())
	{
		TooltipText = AvailableColumns[ColumnIndex]->DisplayName;
	}

	if (InRowDataPointer.IsValid() && ColumnIndex < InRowDataPointer->CellData.Num())
	{
		TooltipText = FText::Format(LOCTEXT("ColumnRowNameFmt", "{0}: {1}"), TooltipText, InRowDataPointer->CellData[ColumnIndex]);
	}

	return TooltipText;
}

float FEasyDataTableEditor::GetRowNumberColumnWidth() const
{
	return RowNumberColumnWidth;
}

void FEasyDataTableEditor::RefreshRowNumberColumnWidth()
{

	TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FTextBlockStyle& CellTextStyle = FAppStyle::GetWidgetStyle<FTextBlockStyle>("DataTableEditor.CellText");
	const float CellPadding = 10.0f;

	for (const FEasyDataTableEditorRowListViewDataPtr& RowData : AvailableRows)
	{
		const float RowNumberWidth = (float)FontMeasure->Measure(FString::FromInt(RowData->RowNum), CellTextStyle.Font).X + CellPadding;
		RowNumberColumnWidth = FMath::Max(RowNumberColumnWidth, RowNumberWidth);
	}

}

void FEasyDataTableEditor::OnRowNumberColumnResized(const float NewWidth)
{
	RowNumberColumnWidth = NewWidth;
}

float FEasyDataTableEditor::GetRowNameColumnWidth() const
{
	return RowNameColumnWidth;
}

void FEasyDataTableEditor::RefreshRowNameColumnWidth()
{
	
	TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FTextBlockStyle& CellTextStyle = FAppStyle::GetWidgetStyle<FTextBlockStyle>("DataTableEditor.CellText");
	static const float CellPadding = 10.0f;

	for (const FEasyDataTableEditorRowListViewDataPtr& RowData : AvailableRows)
	{
		const float RowNameWidth = (float)FontMeasure->Measure(RowData->DisplayName, CellTextStyle.Font).X + CellPadding;
		RowNameColumnWidth = FMath::Max(RowNameColumnWidth, RowNameWidth);
	}
	
}

float FEasyDataTableEditor::GetColumnWidth(const int32 ColumnIndex) const
{
	if (ColumnWidths.IsValidIndex(ColumnIndex))
	{
		return ColumnWidths[ColumnIndex].CurrentWidth;
	}
	return 0.0f;
}

void FEasyDataTableEditor::OnColumnResized(const float NewWidth, const int32 ColumnIndex)
{
	if (ColumnWidths.IsValidIndex(ColumnIndex))
	{
		FColumnWidth& ColumnWidth = ColumnWidths[ColumnIndex];
		ColumnWidth.bIsAutoSized = false;
		ColumnWidth.CurrentWidth = NewWidth;

		// Update the persistent column widths in the layout data
		{
			if (!LayoutData.IsValid())
			{
				LayoutData = MakeShareable(new FJsonObject());
			}

			TSharedPtr<FJsonObject> LayoutColumnWidths;
			if (!LayoutData->HasField(TEXT("ColumnWidths")))
			{
				LayoutColumnWidths = MakeShareable(new FJsonObject());
				LayoutData->SetObjectField(TEXT("ColumnWidths"), LayoutColumnWidths);
			}
			else
			{
				LayoutColumnWidths = LayoutData->GetObjectField(TEXT("ColumnWidths"));
			}

			const FString& ColumnName = AvailableColumns[ColumnIndex]->ColumnId.ToString();
			LayoutColumnWidths->SetNumberField(ColumnName, NewWidth);
		}
	}
}

void FEasyDataTableEditor::OnRowNameColumnResized(const float NewWidth)
{
	RowNameColumnWidth = NewWidth;
}

void FEasyDataTableEditor::LoadLayoutData()
{
	LayoutData.Reset();

	const UDataTable* Table = GetDataTable();
	if (!Table)
	{
		return;
	}

	const FString LayoutDataFilename = FPaths::ProjectSavedDir() / TEXT("AssetData") / TEXT("DataTableEditorLayout") / Table->GetName() + TEXT(".json");

	FString JsonText;
	if (FFileHelper::LoadFileToString(JsonText, *LayoutDataFilename))
	{
		TSharedRef< TJsonReader<TCHAR> > JsonReader = TJsonReaderFactory<TCHAR>::Create(JsonText);
		FJsonSerializer::Deserialize(JsonReader, LayoutData);
	}
}

void FEasyDataTableEditor::SaveLayoutData()
{
	const UDataTable* Table = GetDataTable();
	if (!Table || !LayoutData.IsValid())
	{
		return;
	}

	const FString LayoutDataFilename = FPaths::ProjectSavedDir() / TEXT("AssetData") / TEXT("DataTableEditorLayout") / Table->GetName() + TEXT(".json");

	FString JsonText;
	TSharedRef< TJsonWriter< TCHAR, TPrettyJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy<TCHAR> >::Create(&JsonText);
	if (FJsonSerializer::Serialize(LayoutData.ToSharedRef(), JsonWriter))
	{
		FFileHelper::SaveStringToFile(JsonText, *LayoutDataFilename);
	}
}

TSharedRef<ITableRow> FEasyDataTableEditor::MakeRowWidget(FEasyDataTableEditorRowListViewDataPtr InRowDataPtr, const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(SEasyDataTableListViewRow, OwnerTable)
		.DataTableEditor(SharedThis(this))
		.RowDataPtr(InRowDataPtr)
		.IsEditable(CanEditRows());
}

TSharedRef<SWidget> FEasyDataTableEditor::MakeCellWidget(FEasyDataTableEditorRowListViewDataPtr InRowDataPtr, const int32 InRowIndex, const FName& InColumnId)
{
	int32 ColumnIndex = 0;
	for (; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
	{
		const FEasyDataTableEditorColumnHeaderDataPtr& ColumnData = AvailableColumns[ColumnIndex];
		if (ColumnData->ColumnId == InColumnId)
		{
			break;
		}
	}

	// Valid column ID?
	if (AvailableColumns.IsValidIndex(ColumnIndex) && InRowDataPtr->CellData.IsValidIndex(ColumnIndex))
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2, 4, 2))
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "DataTableEditor.CellText")
				.ColorAndOpacity(this, &FEasyDataTableEditor::GetRowTextColor, InRowDataPtr->RowId)
				.Text(this, &FEasyDataTableEditor::GetCellText, InRowDataPtr, ColumnIndex)
				.HighlightText(this, &FEasyDataTableEditor::GetFilterText)
				.ToolTipText(this, &FEasyDataTableEditor::GetCellToolTipText, InRowDataPtr, ColumnIndex)
			];
	}

	return SNullWidget::NullWidget;
}

void FEasyDataTableEditor::OnRowSelectionChanged(FEasyDataTableEditorRowListViewDataPtr InNewSelection, ESelectInfo::Type InSelectInfo)
{
	const bool bSelectionChanged = !InNewSelection.IsValid() || InNewSelection->RowId != HighlightedRowName;
	const FName NewRowName = (InNewSelection.IsValid()) ? InNewSelection->RowId : NAME_None;

	SetHighlightedRow(NewRowName);
	
	if (bSelectionChanged)
	{
		CallbackOnRowHighlighted.ExecuteIfBound(HighlightedRowName);
	}
}

void FEasyDataTableEditor::CopySelectedRow()
{
	UDataTable* TablePtr = Cast<UDataTable>(GetEditingObject());
	uint8* RowPtr = TablePtr ? TablePtr->GetRowMap().FindRef(HighlightedRowName) : nullptr;

	if (!RowPtr || !TablePtr->RowStruct)
		return;

	FString ClipboardValue;
	TablePtr->RowStruct->ExportText(ClipboardValue, RowPtr, RowPtr, TablePtr, PPF_Copy, nullptr);

	FPlatformApplicationMisc::ClipboardCopy(*ClipboardValue);
}

void FEasyDataTableEditor::PasteOnSelectedRow()
{
	UDataTable* TablePtr = Cast<UDataTable>(GetEditingObject());
	uint8* RowPtr = TablePtr ? TablePtr->GetRowMap().FindRef(HighlightedRowName) : nullptr;

	if (!RowPtr || !TablePtr->RowStruct)
		return;

	const FScopedTransaction Transaction(LOCTEXT("PasteDataTableRow", "Paste Data Table Row"));
	TablePtr->Modify();

	FString ClipboardValue;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardValue);

	FEasyDataTableEditorUtils::BroadcastPreChange(TablePtr, FEasyDataTableEditorUtils::EDataTableChangeInfo::RowData);

	const TCHAR* Result = TablePtr->RowStruct->ImportText(*ClipboardValue, RowPtr, TablePtr, PPF_Copy, GWarn, GetPathNameSafe(TablePtr->RowStruct));

	TablePtr->HandleDataTableChanged(HighlightedRowName);
	TablePtr->MarkPackageDirty();

	FEasyDataTableEditorUtils::BroadcastPostChange(TablePtr, FEasyDataTableEditorUtils::EDataTableChangeInfo::RowData);

	if (Result == nullptr)
	{
		FNotificationInfo Info(LOCTEXT("FailedPaste", "Failed to paste row"));
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void FEasyDataTableEditor::DuplicateSelectedRow()
{
	UDataTable* TablePtr = Cast<UDataTable>(GetEditingObject());
	FName NewName = HighlightedRowName;

	if (NewName == NAME_None || TablePtr == nullptr)
	{
		return;
	}

	const TArray<FName> ExistingNames = TablePtr->GetRowNames();
	while (ExistingNames.Contains(NewName))
	{
		NewName.SetNumber(NewName.GetNumber() + 1);
	}

	FEasyDataTableEditorUtils::DuplicateRow(TablePtr, HighlightedRowName, NewName);
	FEasyDataTableEditorUtils::SelectRow(TablePtr, NewName);
}

void FEasyDataTableEditor::RenameSelectedRowCommand()
{
	UDataTable* TablePtr = Cast<UDataTable>(GetEditingObject());
	FName NewName = HighlightedRowName; 

	if (NewName == NAME_None || TablePtr == nullptr)
	{
		return;
	}

	if (VisibleRows.IsValidIndex(HighlightedVisibleRowIndex))
	{
		TSharedPtr< SEasyDataTableListViewRow > RowWidget = StaticCastSharedPtr< SEasyDataTableListViewRow >(CellsListView->WidgetFromItem(VisibleRows[HighlightedVisibleRowIndex]));
		RowWidget->SetRowForRename();
	}
}

void FEasyDataTableEditor::DeleteSelectedRow()
{
	if (UDataTable* Table = GetEditableDataTable())
	{
		// We must perform this before removing the row
		const int32 RowToRemoveIndex = VisibleRows.IndexOfByPredicate([&](const FEasyDataTableEditorRowListViewDataPtr& InRowName) -> bool
		{
			return InRowName->RowId == HighlightedRowName;
		});
		// Remove row
		if (FEasyDataTableEditorUtils::RemoveRow(Table, HighlightedRowName))
		{
			// Try and keep the same row index selected
			const int32 RowIndexToSelect = FMath::Clamp(RowToRemoveIndex, 0, VisibleRows.Num() - 1);
			if (VisibleRows.IsValidIndex(RowIndexToSelect))
			{
				FEasyDataTableEditorUtils::SelectRow(Table, VisibleRows[RowIndexToSelect]->RowId);
			}
			// Refresh list. Otherwise, the removed row would still appear in the screen until the next list refresh. An
			// analog of CellsListView->RequestListRefresh() also occurs inside FEasyDataTableEditorUtils::SelectRow
			else
			{
				CellsListView->RequestListRefresh();
			}
		}
	}
}

FText FEasyDataTableEditor::GetFilterText() const
{
	return ActiveFilterText;
}

void FEasyDataTableEditor::OnFilterTextChanged(const FText& InFilterText)
{
	ActiveFilterText = InFilterText;
	UpdateVisibleRows();
}

void FEasyDataTableEditor::OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnCleared)
	{
		SearchBoxWidget->SetText(FText::GetEmpty());
		OnFilterTextChanged(FText::GetEmpty());
	}
}

void FEasyDataTableEditor::PostRegenerateMenusAndToolbars()
{
	const UDataTable* DataTable = GetDataTable();

	if (DataTable)
	{
		const UUserDefinedStruct* UDS = Cast<const UUserDefinedStruct>(DataTable->GetRowStruct());

		// build and attach the menu overlay
		TSharedRef<SHorizontalBox> MenuOverlayBox = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.ShadowOffset(FVector2D::UnitVector)
				.Text(LOCTEXT("DataTableEditor_RowStructType", "Row Type: "))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SHyperlink)
				.Style(FAppStyle::Get(), "Common.GotoNativeCodeHyperlink")
				.OnNavigate(this, &FEasyDataTableEditor::OnEditDataTableStructClicked)
				.Text(FText::FromName(DataTable->GetRowStructPathName().GetAssetName()))
				.ToolTipText(LOCTEXT("DataTableRowToolTip", "Open the struct used for each row in this data table"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &FEasyDataTableEditor::OnFindRowInContentBrowserClicked)
				.Visibility(UDS ? EVisibility::Visible : EVisibility::Collapsed)
				.ToolTipText(LOCTEXT("FindRowInCBToolTip", "Find struct in Content Browser"))
				.ContentPadding(4.0f)
				.ForegroundColor(FSlateColor::UseForeground())
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Search"))
				]
			];
	
		SetMenuOverlay(MenuOverlayBox);
	}
}

FReply FEasyDataTableEditor::OnFindRowInContentBrowserClicked()
{
	const UDataTable* DataTable = GetDataTable();
	if(DataTable)
	{
		TArray<FAssetData> ObjectsToSync;
		ObjectsToSync.Add(FAssetData(DataTable->GetRowStruct()));
		GEditor->SyncBrowserToObjects(ObjectsToSync);
	}

	return FReply::Handled();
}

void FEasyDataTableEditor::RefreshCachedDataTable(const FName InCachedSelection, const bool bUpdateEvenIfValid)
{
	UDataTable* Table = GetEditableDataTable();
	TArray<FEasyDataTableEditorColumnHeaderDataPtr> PreviousColumns = AvailableColumns;

	FEasyDataTableEditorUtils::CacheDataTableForEditing(Table, AvailableColumns, AvailableRows);

	// Update the desired width of the row names and numbers column
	// This prevents it growing or shrinking as you scroll the list view
	RefreshRowNumberColumnWidth();
	RefreshRowNameColumnWidth();

	// Setup the default auto-sized columns
	ColumnWidths.SetNum(AvailableColumns.Num());
	for (int32 ColumnIndex = 0; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
	{
		const FEasyDataTableEditorColumnHeaderDataPtr& ColumnData = AvailableColumns[ColumnIndex];
		FColumnWidth& ColumnWidth = ColumnWidths[ColumnIndex];
		ColumnWidth.CurrentWidth = FMath::Clamp(ColumnData->DesiredColumnWidth, 10.0f, 400.0f); // Clamp auto-sized columns to a reasonable limit
	}

	// Load the persistent column widths from the layout data
	{
		const TSharedPtr<FJsonObject>* LayoutColumnWidths = nullptr;
		if (LayoutData.IsValid() && LayoutData->TryGetObjectField(TEXT("ColumnWidths"), LayoutColumnWidths))
		{
			for(int32 ColumnIndex = 0; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
			{
				const FEasyDataTableEditorColumnHeaderDataPtr& ColumnData = AvailableColumns[ColumnIndex];

				double LayoutColumnWidth = 0.0f;
				if ((*LayoutColumnWidths)->TryGetNumberField(ColumnData->ColumnId.ToString(), LayoutColumnWidth))
				{
					FColumnWidth& ColumnWidth = ColumnWidths[ColumnIndex];
					ColumnWidth.bIsAutoSized = false;
					ColumnWidth.CurrentWidth = static_cast<float>(LayoutColumnWidth);
				}
			}
		}
	}

	if (PreviousColumns != AvailableColumns)
	{
		ColumnNamesHeaderRow->ClearColumns();

		if (CanEditRows())
		{
			ColumnNamesHeaderRow->AddColumn(
				SHeaderRow::Column(RowDragDropColumnId)
				[
					SNew(SBox)
					.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.ToolTip(IDocumentation::Get()->CreateToolTip(
					LOCTEXT("DataTableRowHandleTooltip", "Drag Drop Handles"),
					nullptr,
					*FEasyDataTableEditorUtils::VariableTypesTooltipDocLink,
					TEXT("DataTableRowHandle")))
				[
					SNew(STextBlock)
					.Text(FText::GetEmpty())
				]
				]
			);
		}	

		ColumnNamesHeaderRow->AddColumn(
			SHeaderRow::Column(RowNumberColumnId)
			.SortMode(this, &FEasyDataTableEditor::GetColumnSortMode, RowNumberColumnId)
			.OnSort(this, &FEasyDataTableEditor::OnColumnNumberSortModeChanged)
			.ManualWidth(this, &FEasyDataTableEditor::GetRowNumberColumnWidth)
			.OnWidthChanged(this, &FEasyDataTableEditor::OnRowNumberColumnResized)
			[
				SNew(SBox)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.ToolTip(IDocumentation::Get()->CreateToolTip(
				LOCTEXT("DataTableRowIndexTooltip", "Row Index"),
				nullptr,
				*FEasyDataTableEditorUtils::VariableTypesTooltipDocLink,
				TEXT("DataTableRowIndex")))
				[
					SNew(STextBlock)
					.Text(FText::GetEmpty())
				]
			]

		);

		ColumnNamesHeaderRow->AddColumn(
			SHeaderRow::Column(RowNameColumnId)
			.DefaultLabel(LOCTEXT("DataTableRowName", "Row Name"))
			.ManualWidth(this, &FEasyDataTableEditor::GetRowNameColumnWidth)
			.OnWidthChanged(this, &FEasyDataTableEditor::OnRowNameColumnResized)
			.SortMode(this, &FEasyDataTableEditor::GetColumnSortMode, RowNameColumnId)
			.OnSort(this, &FEasyDataTableEditor::OnColumnNameSortModeChanged)
		);

		for (int32 ColumnIndex = 0; ColumnIndex < AvailableColumns.Num(); ++ColumnIndex)
		{
			const FEasyDataTableEditorColumnHeaderDataPtr& ColumnData = AvailableColumns[ColumnIndex];

			ColumnNamesHeaderRow->AddColumn(
				SHeaderRow::Column(ColumnData->ColumnId)
				.DefaultLabel(ColumnData->DisplayName)
				.ManualWidth(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateSP(this, &FEasyDataTableEditor::GetColumnWidth, ColumnIndex)))
				.OnWidthChanged(this, &FEasyDataTableEditor::OnColumnResized, ColumnIndex)
				.SortMode(this, &FEasyDataTableEditor::GetColumnSortMode, ColumnData->ColumnId)
				.OnSort(this, &FEasyDataTableEditor::OnColumnSortModeChanged)
				[
					SNew(SBox)
					.Padding(FMargin(0, 4, 0, 4))
					.VAlign(VAlign_Fill)
					.ToolTip(IDocumentation::Get()->CreateToolTip(FEasyDataTableEditorUtils::GetRowTypeInfoTooltipText(ColumnData), nullptr, *FEasyDataTableEditorUtils::VariableTypesTooltipDocLink, FEasyDataTableEditorUtils::GetRowTypeTooltipDocExcerptName(ColumnData)))
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(ColumnData->DisplayName)
					]
				]
			);
		}
	}

	UpdateVisibleRows(InCachedSelection, bUpdateEvenIfValid);

	if (PropertyView.IsValid())
	{
		PropertyView->SetObject(Table);
	}
}

void FEasyDataTableEditor::UpdateVisibleRows(const FName InCachedSelection, const bool bUpdateEvenIfValid)
{
	if (ActiveFilterText.IsEmptyOrWhitespace())
	{
		VisibleRows = AvailableRows;
	}
	else
	{
		VisibleRows.Empty(AvailableRows.Num());

		const FString& ActiveFilterString = ActiveFilterText.ToString();
		for (const FEasyDataTableEditorRowListViewDataPtr& RowData : AvailableRows)
		{
			bool bPassesFilter = false;

			if (RowData->DisplayName.ToString().Contains(ActiveFilterString))
			{
				bPassesFilter = true;
			}
			else
			{
				for (const FText& CellText : RowData->CellData)
				{
					if (CellText.ToString().Contains(ActiveFilterString))
					{
						bPassesFilter = true;
						break;
					}
				}
			}

			if (bPassesFilter)
			{
				VisibleRows.Add(RowData);
			}
		}
	}

	CellsListView->RequestListRefresh();
	RestoreCachedSelection(InCachedSelection, bUpdateEvenIfValid);
}

void FEasyDataTableEditor::RestoreCachedSelection(const FName InCachedSelection, const bool bUpdateEvenIfValid)
{
	// Validate the requested selection to see if it matches a known row
	bool bSelectedRowIsValid = false;
	if (!InCachedSelection.IsNone())
	{
		bSelectedRowIsValid = VisibleRows.ContainsByPredicate([&InCachedSelection](const FEasyDataTableEditorRowListViewDataPtr& RowData) -> bool
		{
			return RowData->RowId == InCachedSelection;
		});
	}

	// Apply the new selection (if required)
	if (!bSelectedRowIsValid)
	{
		SetHighlightedRow((VisibleRows.Num() > 0) ? VisibleRows[0]->RowId : NAME_None);
		CallbackOnRowHighlighted.ExecuteIfBound(HighlightedRowName);
	}
	else if (bUpdateEvenIfValid)
	{
		SetHighlightedRow(InCachedSelection);
		CallbackOnRowHighlighted.ExecuteIfBound(HighlightedRowName);
	}
}

TSharedRef<SVerticalBox> FEasyDataTableEditor::CreateContentBox()
{
	TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(12.0f, 12.0f));

	TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(12.0f, 12.0f));

	ColumnNamesHeaderRow = SNew(SHeaderRow);

	CellsListView = SNew(SListView<FEasyDataTableEditorRowListViewDataPtr>)
		.ListItemsSource(&VisibleRows)
		.HeaderRow(ColumnNamesHeaderRow)
		.OnGenerateRow(this, &FEasyDataTableEditor::MakeRowWidget)
		.OnSelectionChanged(this, &FEasyDataTableEditor::OnRowSelectionChanged)
		.ExternalScrollbar(VerticalScrollBar)
		.ConsumeMouseWheel(EConsumeMouseWheel::Always)
		.SelectionMode(ESelectionMode::Type::Multi)
		.AllowOverscroll(EAllowOverscroll::No);
	
	LoadLayoutData();
	RefreshCachedDataTable();

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SAssignNew(SearchBoxWidget, SSearchBox)
				.InitialText(this, &FEasyDataTableEditor::GetFilterText)
				.OnTextChanged(this, &FEasyDataTableEditor::OnFilterTextChanged)
				.OnTextCommitted(this, &FEasyDataTableEditor::OnFilterTextCommitted)
			]
		]
		+SVerticalBox::Slot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				.ExternalScrollbar(HorizontalScrollBar)
				+SScrollBox::Slot()
				[
					CellsListView.ToSharedRef()
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				VerticalScrollBar
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				HorizontalScrollBar
			]
		];
}

TSharedRef<SWidget> FEasyDataTableEditor::CreateRowEditorBox()
{
	UDataTable* Table = Cast<UDataTable>(GetEditingObject());

	// Support undo/redo
	if (Table)
	{
		Table->SetFlags(RF_Transactional);
	}
	
	auto RowEditor = SNew(SEasyRowEditor, Table,SharedThis(this).ToWeakPtr());
	RowEditor->RowSelectedCallback.BindSP(this, &FEasyDataTableEditor::SetHighlightedRow);
	CallbackOnRowHighlighted.BindSP(RowEditor, &SEasyRowEditor::SelectRow);
	CallbackOnDataTableUndoRedo.BindSP(RowEditor, &SEasyRowEditor::HandleUndoRedo);
	return RowEditor;
}

TSharedRef<SEasyRowEditor> FEasyDataTableEditor::CreateRowEditor(UDataTable* Table)
{
	return SNew(SEasyRowEditor, Table, SharedThis(this).ToWeakPtr());
}

TSharedRef<SDockTab> FEasyDataTableEditor::SpawnTab_RowEditor(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == RowEditorTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("RowEditorTitle", "Row Editor"))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SBorder)
			.Padding(2)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				RowEditorTabWidget.ToSharedRef()
			]
		];
}


TSharedRef<SDockTab> FEasyDataTableEditor::SpawnTab_DataTable( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == DataTableTabId );

	UDataTable* Table = Cast<UDataTable>(GetEditingObject());

	// Support undo/redo
	if (Table)
	{
		Table->SetFlags(RF_Transactional);
	}

	return SNew(SDockTab)
		.Label( LOCTEXT("DataTableTitle", "Data Table") )
		.TabColorScale( GetTabColorScale() )
		[
			SNew(SBorder)
			.Padding(2)
			.BorderImage( FAppStyle::GetBrush( "ToolPanel.GroupBorder" ) )
			[
				DataTableTabWidget.ToSharedRef()
			]
		];
}

TSharedRef<SDockTab> FEasyDataTableEditor::SpawnTab_DataTableDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == DataTableDetailsTabId);

	PropertyView->SetObject(GetEditableDataTable());

	return SNew(SDockTab)
		.Label(LOCTEXT("DataTableDetails", "Data Table Details"))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SBorder)
			.Padding(2)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				PropertyView.ToSharedRef()
			]
		];
}

void FEasyDataTableEditor::SetHighlightedRow(FName Name)
{
	if (Name == HighlightedRowName)
	{
		return;
	}

	if (Name.IsNone())
	{
		HighlightedRowName = NAME_None;
		CellsListView->ClearSelection();
		HighlightedVisibleRowIndex = INDEX_NONE;
	}
	else
	{
		HighlightedRowName = Name;

		FEasyDataTableEditorRowListViewDataPtr* NewSelectionPtr = NULL;
		for (HighlightedVisibleRowIndex = 0; HighlightedVisibleRowIndex < VisibleRows.Num(); ++HighlightedVisibleRowIndex)
		{
			if (VisibleRows[HighlightedVisibleRowIndex]->RowId == Name)
			{
				NewSelectionPtr = &(VisibleRows[HighlightedVisibleRowIndex]);

				break;
			}
		}


		// Synchronize the list views
		if (NewSelectionPtr)
		{
			//修改后的高光
			//CellsListView->SetItemSelection(*NewSelectionPtr,true);//->SetSelection(*NewSelectionPtr);
			CellsListView->SetItemHighlighted(*NewSelectionPtr,true);
			CellsListView->RequestScrollIntoView(*NewSelectionPtr);
		}
		else
		{
			CellsListView->ClearSelection();
		}
	}
}

#undef LOCTEXT_NAMESPACE
