#pragma once

#pragma once

#include "CoreMinimal.h"
#include "DataTableEditorUtils.h"
#include "Engine/DataTable.h"
#include "Kismet2/ListenerManager.h"
#include "Widgets/SWidget.h"
#include "Framework/Commands/UIAction.h"
#include "AssetRegistry/AssetData.h"

struct FEasyDataTableEditorColumnHeaderData
{
	/** Unique ID used to identify this column */
	FName ColumnId;

	/** Display name of this column */
	FText DisplayName;

	/** The calculated width of this column taking into account the cell data for each row */
	float DesiredColumnWidth;

	/** The FProperty for the variable in this column */
	const FProperty* Property;
};

struct FEasyDataTableEditorRowListViewData
{
	/** Unique ID used to identify this row */
	FName RowId;

	/** Display name of this row */
	FText DisplayName;

	/** The calculated height of this row taking into account the cell data for each column */
	float DesiredRowHeight;

	/** Insertion number of the row */
	uint32 RowNum;

	/** Array corresponding to each cell in this row */
	TArray<FText> CellData;
};

typedef TSharedPtr<FEasyDataTableEditorColumnHeaderData> FEasyDataTableEditorColumnHeaderDataPtr;
typedef TSharedPtr<FEasyDataTableEditorRowListViewData>  FEasyDataTableEditorRowListViewDataPtr;


struct FEasyDataTableEditorUtils
{
	enum class EDataTableChangeInfo
	{
		/** The data corresponding to a single row has been changed */
		RowData,
		/** The data corresponding to the entire list of rows has been changed */
		RowList,
	};

	enum class ERowMoveDirection
	{
		Up,
		Down,
	};

	class FEasyDataTableEditorManager : public FListenerManager < UDataTable, EDataTableChangeInfo >
	{
		FEasyDataTableEditorManager() {}
	public:
		EASYDATATABLEEDITOR_API static FEasyDataTableEditorManager& Get();

		class ListenerType : public InnerListenerType<FEasyDataTableEditorManager>
		{
		public:
			virtual void SelectionChange(const UDataTable* DataTable, FName RowName) { }
		};
	};

	typedef FEasyDataTableEditorManager::ListenerType INotifyOnDataTableChanged;

	static EASYDATATABLEEDITOR_API bool RemoveRow(UDataTable* DataTable, FName Name);
	static EASYDATATABLEEDITOR_API uint8* AddRow(UDataTable* DataTable, FName RowName);
	static EASYDATATABLEEDITOR_API uint8* DuplicateRow(UDataTable* DataTable, FName SourceRowName, FName RowName);
	static EASYDATATABLEEDITOR_API bool RenameRow(UDataTable* DataTable, FName OldName, FName NewName);
	static EASYDATATABLEEDITOR_API bool MoveRow(UDataTable* DataTable, FName RowName, ERowMoveDirection Direction, int32 NumRowsToMoveBy = 1);
	static EASYDATATABLEEDITOR_API bool SelectRow(const UDataTable* DataTable, FName RowName);
	static EASYDATATABLEEDITOR_API bool DiffersFromDefault(UDataTable* DataTable, FName RowName);
	static EASYDATATABLEEDITOR_API bool ResetToDefault(UDataTable* DataTable, FName RowName);

	static EASYDATATABLEEDITOR_API uint8* AddRowAboveOrBelowSelection(UDataTable* DataTable, const FName& RowName, const FName& NewRowName, ERowInsertionPosition InsertPosition);

	static EASYDATATABLEEDITOR_API void BroadcastPreChange(UDataTable* DataTable, EDataTableChangeInfo Info);
	static EASYDATATABLEEDITOR_API void BroadcastPostChange(UDataTable* DataTable, EDataTableChangeInfo Info);
	static EASYDATATABLEEDITOR_API void BroadcastPostRowPropertyChange(UDataTable* DataTable, const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged, TSharedPtr<class SEasyRowEditor> EasyRowEditor);

	/** Reads a data table and parses out editable copies of rows and columns */
	static EASYDATATABLEEDITOR_API void CacheDataTableForEditing(const UDataTable* DataTable, TArray<FEasyDataTableEditorColumnHeaderDataPtr>& OutAvailableColumns, TArray<FEasyDataTableEditorRowListViewDataPtr>& OutAvailableRows);

	/** Generic version that works with any datatable-like structure */
	static EASYDATATABLEEDITOR_API void CacheDataForEditing(const UScriptStruct* RowStruct, const TMap<FName, uint8*>& RowMap, TArray<FEasyDataTableEditorColumnHeaderDataPtr>& OutAvailableColumns, TArray<FEasyDataTableEditorRowListViewDataPtr>& OutAvailableRows);

	/** Returns all script structs that can be used as a data table row. This only includes loaded ones */
	static EASYDATATABLEEDITOR_API TArray<UScriptStruct*> GetPossibleStructs();

	/** Fills in an array with all possible DataTable structs, unloaded and loaded */
	static EASYDATATABLEEDITOR_API void GetPossibleStructAssetData(TArray<FAssetData>& StructAssets);
	
	/** Utility function which verifies that the specified struct type is viable for data tables */
	static EASYDATATABLEEDITOR_API bool IsValidTableStruct(const UScriptStruct* Struct);

	/** Add a UI action for search for references, useful for customizations */
	static EASYDATATABLEEDITOR_API void AddSearchForReferencesContextMenu(class FDetailWidgetRow& RowNameDetailWidget, FExecuteAction SearchForReferencesAction);

	/** Short description for a data or curve handle */
	static EASYDATATABLEEDITOR_API FText GetHandleShortDescription(const UObject* TableAsset, FName RowName);

	/** Tooltip text for the data table row type */
	static EASYDATATABLEEDITOR_API FText GetRowTypeInfoTooltipText(FEasyDataTableEditorColumnHeaderDataPtr ColumnHeaderDataPtr);

	/** Doc excerpt name for the data table row type */
	static EASYDATATABLEEDITOR_API FString GetRowTypeTooltipDocExcerptName(FEasyDataTableEditorColumnHeaderDataPtr ColumnHeaderDataPtr);

	/** Link to variable type doc  */
	static EASYDATATABLEEDITOR_API const FString VariableTypesTooltipDocLink;

	/** Delegate called when a data table struct is selected */
	DECLARE_DELEGATE_OneParam(FOnDataTableStructSelected, UScriptStruct*);

	/** Creates a combo box that allows selecting from the list of possible row structures */
	static EASYDATATABLEEDITOR_API TSharedRef<SWidget> MakeRowStructureComboBox(FOnDataTableStructSelected OnSelected);
};
