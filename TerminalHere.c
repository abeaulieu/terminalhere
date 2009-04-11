/*
 *  TerminalHere - A contextual menu plugin to open a terminal to the selected folder.
 *  Copyright (C) 2009 Alexandre Beaulieu
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "TerminalHere.h"

// 01694193-E77D-4D5B-9385-2075BC188C80
#define kCMPlugInFactoryId (CFUUIDGetConstantUUIDWithBytes \
   (NULL, \
    0x01, 0x69, 0x41, 0x93, \
    0xE7, 0x7D, \
    0x4D, 0x5B, \
    0x93, 0x85, \
    0x20, 0x75, 0xBC, 0x18, 0x8C, 0x80))

// The Open Terminal Here menu's associated command id.
#define kSeparatorMenuCommandId     (7000L)
#define kTerminalHereMenuCommandId  (7001L)

// Static functions declarations
static ULONG addRef(void *pluginInstance);
static ULONG release(void *pluginInstance);
static void deallocateInstance (TerminalHerePlugin *pluginInstance);

void* terminalHereFactory (CFAllocatorRef allocator, CFUUIDRef typeId);
static TerminalHerePlugin* allocateInstance (CFUUIDRef factoryId);

static HRESULT queryInterface(void* pluginInstance, REFIID iid, LPVOID* ppv);
static Boolean isValidInterface(REFIID iid);

static OSStatus examineContext(void *pluginInstance, const AEDesc *context, AEDescList *commandList);
static OSStatus getSelectedFile(FSRef *file, const AEDesc *desc);
static Boolean getFSRefFromAEDesc(FSRef *ref, const AEDesc *desc);
static CFStringRef createFileNameFromFSRef(const FSRef *ref);

static OSStatus appendMenuSeparator(AEDescList *commandList);
static OSStatus appendMenuItem(AEDescList *commandList, CFStringRef menuName, long commandId);

static Boolean isDirectory(const FSRef *ref);

static void openTerminal(const FSRef *ref);

static OSStatus handleSelection(void *pluginInstance, AEDesc *context, SInt32 commandId);

static void postMenuCleanup(void *pluginInstance);

static char *createUtf8StringFromCFString(CFStringRef cfString);

#ifdef __DEBUG__
static void printCFString(CFStringRef cfString);
#endif // __DEBUG__

// Plugin interface definition
static ContextualMenuInterfaceStruct terminalHereInterface = {
   NULL,
   queryInterface,
   addRef,
   release,
   examineContext,
   handleSelection,
   postMenuCleanup
};

// IUnknown::addRef   
static ULONG addRef(void *pluginInstance)
{
  TerminalHerePlugin *instance = (TerminalHerePlugin*) pluginInstance;
  DEBUG_PRINT("Terminal Here: addRef\n");
  instance->refCount++;
  return instance->refCount;
}

// IUnknown::release
static ULONG release(void *pluginInstance)
{
  TerminalHerePlugin *instance = (TerminalHerePlugin*) pluginInstance;
  DEBUG_PRINT("Terminal Here: release\n");
  instance->refCount--;
  if (instance->refCount == 0)
  {
    deallocateInstance(instance);
    return 0;
  }
  return instance->refCount;
}

static void deallocateInstance (TerminalHerePlugin *pluginInstance)
{
  CFUUIDRef factoryId = pluginInstance->factoryId;
  free(pluginInstance);

  if (factoryId) {
    CFPlugInRemoveInstanceForFactory(factoryId);
    CFRelease(factoryId);
  } 
}

void* terminalHereFactory (CFAllocatorRef allocator, CFUUIDRef typeId)
{
  #pragma unused (allocator)
  DEBUG_PRINT("In Terminal Here Factory\n");
  if (CFEqual (typeId, kContextualMenuTypeID))
    return (allocateInstance (kCMPlugInFactoryId));
  return NULL;
}

static TerminalHerePlugin* allocateInstance (CFUUIDRef factoryId)
{
  TerminalHerePlugin *newInstance;
  newInstance = (TerminalHerePlugin*) malloc(sizeof(TerminalHerePlugin));
  newInstance->cmInterface = &terminalHereInterface;
  newInstance->factoryId = CFRetain(factoryId);   
  CFPlugInAddInstanceForFactory(factoryId);
  newInstance->refCount = 1;

  return newInstance;
}

// IUnknown::queryInterface
static HRESULT queryInterface(void* pluginInstance, REFIID iid, LPVOID* ppv)
{
  DEBUG_PRINT("Terminal Here: queryInterface\n");
  if (isValidInterface(iid)) {
    addRef(pluginInstance);
    *ppv = pluginInstance;
    return S_OK;
  }
  *ppv = NULL;
  return E_NOINTERFACE;
}

static Boolean isValidInterface(REFIID iid)
{
  CFUUIDRef interfaceId = CFUUIDCreateFromUUIDBytes (NULL, iid);
  Boolean flGoodInterface = false;
  
  if (CFEqual (interfaceId, kContextualMenuInterfaceID))
    flGoodInterface = true;
  if (CFEqual (interfaceId, IUnknownUUID))
    flGoodInterface = true;

  CFRelease (interfaceId);
  return (flGoodInterface);
}



//================= End COM Plugin Definition ======================


//=============== Terminal Here Implementation =====================

static OSStatus examineContext(void *pluginInstance, const AEDesc *context, AEDescList *commandList)
{
  FSRef file;
  OSStatus err;
  
#ifdef __DEBUG__
  CFStringRef fileName = NULL;
#endif
  
  DEBUG_PRINT("Terminal Here: examineContext\n");
  
  err = getSelectedFile(&file, context);
  if (err != noErr)
  {
    DEBUG_PRINT("Terminal Here: examineContext no file to act on");
    return err;
  }
  
#ifdef __DEBUG__
  fileName = createFileNameFromFSRef(&file);
  DEBUG_PRINT("Terminal Here: File name is ");
  DEBUG_CFSTR(fileName);
  CFRelease(fileName);
#endif

  if (isDirectory(&file))
  {
    appendMenuSeparator(commandList);
    appendMenuItem(commandList, CFSTR("Open Terminal Here"), kTerminalHereMenuCommandId);
  }
  else {
    DEBUG_PRINT("Terminal Here: Not a directory\n");
  }
  
  DEBUG_PRINT("Terminal Here: examineContext end\n");
  
  return (noErr);
}

/**
 * Returns the selected file from the AEDesc.
 * @param file The FSRef to set to the file. In case of error, the value in file is undefined.
 * @param desc The AEDesc to inspect for a selected file.
 * @return An error code or noErr if no error occured.
 */
static OSStatus getSelectedFile(FSRef *file, const AEDesc *desc)
{
  AEDesc descItem;
  AEKeyword keyword;
  long count = 0;
  
  OSStatus err = noErr;
  
  // 1 or more files in this list
  if (desc->descriptorType == typeAEList) {
  
    DEBUG_PRINT("Terminal Here: getSelectedFile typeAEList\n");
    
    // List of AEDesc.
    err = AECountItems(desc, &count);
    require_noerr(err, getSelectedFile_error);
    
    DEBUG_PRINT("Terminal Here: getSelectedFile %ld items\n", count);
    
    if (count > 1)
    {
      DEBUG_PRINT("Terminal Here: getSelectedFile too many item\n");
      err = errAEWrongDataType;
      goto getSelectedFile_error;
    }
    
    err = AEGetNthDesc(desc, 1, typeWildCard, &keyword, &descItem);
    require_noerr(err, getSelectedFile_error);
      
    if (!getFSRefFromAEDesc(file, &descItem)) {
      AEDisposeDesc(&descItem);
      goto getSelectedFile_error;
    }
      
    AEDisposeDesc(&descItem);
  }
  else
  {
    DEBUG_PRINT("Terminal Here: getSelectedFile not an AEList\n");
    
    if (!getFSRefFromAEDesc(file, desc)) {
      DEBUG_PRINT("Terminal Here: getSelectedFile couldn't get from AEDesc\n");
      err = errAEWrongDataType;
      goto getSelectedFile_error;
    }
  }
  
getSelectedFile_error:
  return err;
}

/**
 * Gets a FSRef from an AEDesc that can be coerced to typeAlias.
 * @param ref Will be set to the FSRef of the desc.
 * @param descRef The AEDesc to extract the FSRef from.
 */
static Boolean getFSRefFromAEDesc(FSRef *ref, const AEDesc *descRef)
{
  AliasHandle alias = NULL;
  Size dataSize = 0;
  
  AEDesc coercedDesc;
  const AEDesc *descToUse = descRef;
  
  Boolean changed = false;
  
  OSStatus err = noErr;
  
  DEBUG_PRINT("Terminal Here: getFSRefFromAEDesc\n");
  
  // When it's not an alias, try to coerce it to an alias.
  if (descRef->descriptorType != typeAlias) {
    err = AECoerceDesc(descRef, typeAlias, &coercedDesc);
    require_noerr(err, getFSRefFromAEDesc_error);
    
    descToUse = &coercedDesc;
    
    if (coercedDesc.descriptorType != typeAlias)
      goto getFSRefFromAEDesc_error;
  }
  
  dataSize = AEGetDescDataSize(descToUse);
  
  DEBUG_PRINT("Terminal Here: getFSRefFromAEDesc dataSize=%ld\n", dataSize);
  
  // Create the handle to the file alias.
  alias = (AliasHandle)NewHandle(dataSize);
  if (alias == NULL)
    goto getFSRefFromAEDesc_error;
  
  DEBUG_PRINT("Terminal Here: getFSRefFromAEDesc alias created\n");
  
  // Write the data to the handle.
  err = AEGetDescData(descToUse, *alias, dataSize);
  require_noerr (err, getFSRefFromAEDesc_error);
  
  DEBUG_PRINT("Terminal Here: getFSRefFromAEDesc data read\n");
  
  // Get the target as a FSRef of this alias handle.
  err = FSResolveAlias(NULL, alias, ref, &changed);
  require_noerr (err, getFSRefFromAEDesc_error);
  
  DEBUG_PRINT("Terminal Here: getFSRefFromAEDesc alias resolved\n");
  
  DisposeHandle((Handle)alias);
  
  AEDisposeDesc(&coercedDesc);
  
  DEBUG_PRINT("Terminal Here: getFSRefFromAEDesc success\n");
  
  return true;
  
getFSRefFromAEDesc_error:
  
  DEBUG_PRINT("Terminal Here: getFSRefFromAEDesc failure %ld\n", err);
  
  if (alias != NULL)
    DisposeHandle((Handle)alias);
  
  if (descToUse == &coercedDesc)
    AEDisposeDesc(&coercedDesc);
    
  return false;
}

/**
 * Converts a FSRef to its POSIX representation in a CFStringRef.
 * The caller will need to call CFRelease on the string.
 * @param ref The file name to get the name of.
 * @return An allocated CFStringRef of the file name.
 */
static CFStringRef createFileNameFromFSRef(const FSRef *ref)
{
  CFStringRef cfString;
  CFURLRef url = CFURLCreateFromFSRef( kCFAllocatorDefault, ref );
  if ( url != NULL ) {
    cfString = CFURLCopyFileSystemPath( url, kCFURLPOSIXPathStyle );
    CFRelease( url );
  }
  return cfString;
}

/**
 * Appends a menu separator to the command list.
 * @param commandList The command list to append to.
 */
static OSStatus appendMenuSeparator(AEDescList *commandList)
{
  return appendMenuItem(commandList, CFSTR("-"), kSeparatorMenuCommandId);
}

/**
 * Appends a menu separator to the command list.
 * @param commandList The command list to append to.
 * @param menuName The name to display on the menu.
 * @param commandId The ID to assign to the menu item.
 */
static OSStatus appendMenuItem(AEDescList *commandList, CFStringRef menuName, SInt32 commandId)
{
  AERecord command = {typeNull, NULL};
  OSErr err = noErr;

  // Creates the AERecord to hold the command.
  err = AECreateList(NULL, 0, true, &command);
  require_noerr (err, appendMenuItem_fail);
  
  // Put the menu name in the record.
  err = AEPutKeyPtr(&command, keyAEName, typeCFStringRef, &menuName, sizeof(menuName));
  require_noerr (err, appendMenuItem_fail);

  // Put the command id in the record.
  err = AEPutKeyPtr(&command, keyContextualMenuCommandID, typeLongInteger, &commandId, sizeof(commandId));
  require_noerr (err, appendMenuItem_fail);
  
  // Append
  err = AEPutDesc(commandList, 0, &command); 

  AEDisposeDesc(&command);
  return err;
  
appendMenuItem_fail:
  if (command.descriptorType != typeNull)
    AEDisposeDesc(&command);
  
  return err;
}

/**
 * Checks whether the FSRef represents a directory or not.
 */
static Boolean isDirectory(const FSRef *ref)
{
  OSStatus err = noErr;
  FSCatalogInfo info;
  
  err = FSGetCatalogInfo(ref, kFSCatInfoNodeFlags, &info, NULL, NULL, NULL);
  if (err != noErr)
  {
    return false;
  }
  
  return ((info.nodeFlags & kFSNodeIsDirectoryMask) == kFSNodeIsDirectoryMask);
}

/**
 * Opens the Terminal application and cd to the directory.
 * @param ref The FSRef to the directory to cd to.
 */
static void openTerminal(const FSRef *ref)
{
  FSRef terminalApp;
  LSLaunchFSRefSpec launchSpec;
  
  AppleEvent cdEvent = {typeNull, nil};
  AEDesc cdEventDesc = {typeNull, nil};
  
  const char *terminalBundleId = "com.apple.Terminal";
  char *commandUtf8 = NULL;
  
  CFStringRef pathStr = NULL;
  CFMutableStringRef commandStr = NULL;
  OSStatus err;
  
  DEBUG_PRINT("Terminal Here: Open terminal\n");
  
  // Find the Terminal application.
  err = LSFindApplicationForInfo(
    kLSUnknownCreator,
    CFSTR("com.apple.Terminal"),
    CFSTR("Terminal.app"),
    &terminalApp,
    NULL);
  require_noerr(err, openTerminal_error);
  
  DEBUG_PRINT("Terminal Here: Terminal application found\n");

  // Launch the terminal app or bring it to the front.
  launchSpec.appRef = &terminalApp;
  launchSpec.numDocs = 0;
  launchSpec.itemRefs = NULL;
  launchSpec.passThruParams = NULL;
  launchSpec.launchFlags = kLSLaunchDefaults;
  launchSpec.asyncRefCon = NULL;
  err = LSOpenFromRefSpec(&launchSpec, NULL);
  require_noerr(err, openTerminal_error);
  
  DEBUG_PRINT("Terminal Here: Terminal launched\n");
  
  // Create the description to put in the cd AppleEvent.
  err = AECreateDesc(typeApplicationBundleID, terminalBundleId, strlen(terminalBundleId), &cdEventDesc);
  require_noerr(err, openTerminal_error);
  
  DEBUG_PRINT("Terminal Here: cd event desc created\n");
  
  // Create the script AppleEvent to cd to the folder.
  err = AECreateAppleEvent(
    kAECoreSuite,
    kAEDoScript,
    &cdEventDesc,
    kAutoGenerateReturnID, 
    kAnyTransactionID, 
    &cdEvent);
  AEDisposeDesc(&cdEventDesc);
  require_noerr(err, openTerminal_error);

  DEBUG_PRINT("Terminal Here: Apple Event created\n");
  
  // Create the command string.
  pathStr = createFileNameFromFSRef(ref);
  commandStr = CFStringCreateMutable(kCFAllocatorDefault, 0);
  CFStringAppend(commandStr, CFSTR("cd \""));
  CFStringAppend(commandStr, pathStr); 
  CFStringAppend(commandStr, CFSTR("\""));
  CFRelease(pathStr);
  
  DEBUG_PRINT("Terminal Here: Command to run: ");
  DEBUG_CFSTR(commandStr);
  
  // Create the command parameter to the AppleEvent.
  AEDesc parameters = {typeNull, nil};
  commandUtf8 = createUtf8StringFromCFString(commandStr);
  if (commandUtf8 == NULL) {
    DEBUG_PRINT("Terminal Here: couldn't get UTF8 string ptr from CFStringRef\n");
    goto openTerminal_error;
  }
  err = AECreateDesc(typeUTF8Text, commandUtf8, strlen(commandUtf8), &parameters);
  require_noerr(err, openTerminal_error);

  DEBUG_PRINT("Terminal Here: parameters created\n");
  
  // Add the command parameter to the AppleEvent.
  err = AEPutParamDesc(&cdEvent, kAECommandClass, &parameters);
  AEDisposeDesc(&parameters);
  require_noerr(err, openTerminal_error);
  
  // Send to the AppleEvent to the Terminal application. Ignores reply.
  AppleEvent replyEvent = {typeNull, nil};
  err = AESend(&cdEvent, &replyEvent, kAENoReply, kAENormalPriority, kAEDefaultTimeout, NULL, NULL);
  
  DEBUG_PRINT("Terminal Here: event sent\n");
  
  AEDisposeDesc(&replyEvent);
  
openTerminal_error:
  if (err == noErr) {
    DEBUG_PRINT("Terminal Here: Terminal successfully launched\n");
  }
  else {
    DEBUG_PRINT("Terminal Here: openTerminal error = %ld\n", err);
  }
  
  if (cdEvent.descriptorType != typeNull)
    AEDisposeDesc(&cdEvent);
  
  if (commandStr != NULL)
    CFRelease(commandStr);
    
  if (commandUtf8 != NULL)
    free(commandUtf8);
    
  return;
}

static OSStatus handleSelection(void *pluginInstance, AEDesc *context, SInt32 commandId)
{
  FSRef file;
  OSStatus err;
  
  DEBUG_PRINT("Terminal Here: handleSelection\n");
  
  if (commandId == kTerminalHereMenuCommandId) {
    err = getSelectedFile(&file, context);
    if (err == noErr)
      openTerminal(&file);
  }
  return noErr;
}

static void postMenuCleanup(void *pluginInstance)
{
  // Nothing to do.
  DEBUG_PRINT("Terminal Here: postMenuCleanup\n");
}

/**
 * Create a NUL-terminated UTF-8 encoded C string from a CFString.
 * The caller is responsible for releasing allocated memory for the string.
 * @param cfString The string to convert to UTF-8 C string.
 * @return NULL in case of error or the allocated C String with valid content.
 */
static char *createUtf8StringFromCFString(CFStringRef cfString)
{
  long charLength = CFStringGetLength(cfString);
  long byteSize = CFStringGetMaximumSizeForEncoding(charLength, kCFStringEncodingUTF8);
  
  char *utf8String = (char *)malloc(byteSize+1);
  if (utf8String == NULL) {
    return NULL;
  }
  
  memset(utf8String, 0, byteSize+1);
  
  if (!CFStringGetCString(cfString, utf8String, byteSize, kCFStringEncodingUTF8)) {
    free(utf8String);
    return NULL;
  }
  
  return utf8String;
}

/**
 * Prints a CFString to standard output in ASCII format.
 */
#ifdef __DEBUG__
static void printCFString(CFStringRef cfString)
{
  char *buffer = createUtf8StringFromCFString(cfString);
  
  if (buffer == NULL) {
    printf("printCFString couldn't get C string representation of CFString !\n");
    return;
  }
  
  printf("%s\n", buffer);
  
  free(buffer);
}
#endif // __DEBUG__
