/** @page exampleWeb-SimpleHtml Simple HTML

# NetBurner Simple HTML Example

## Overview

The Simple HTML Example is a basic demonstration of the NetBurner web server implementation and serves as an excellent starting point for custom embedded web applications. This example creates a minimal web server that displays a "Hello World" type message on the device's index page.

## Application Description

This embedded web application initializes a NetBurner device to serve HTML content over HTTP. The application demonstrates the fundamental components needed to create a web-enabled embedded system using the NetBurner Real-Time Operating System (NBRTOS).

### Key Features

- **Web Server**: Runs an HTTP server on the default port 80
- **Network Stack**: Initializes the complete network stack for web connectivity
- **System Diagnostics**: Includes optional system diagnostic capabilities
- **Real-Time Operation**: Built on NBRTOS for reliable real-time performance
- **Simple Interface**: Displays a basic HTML page with NetBurner branding

## File Structure

```
/
    main.cpp         - Main application entry point
    index.html       - Default web page served by the application
    style.css        - CSS styling for the web interface
```

## How It Works

### Application Flow

1. **Initialization**: The `UserMain()` function initializes the network stack
2. **Diagnostics**: Enables system diagnostics for debugging (should be removed in production)
3. **Web Server**: Starts the HTTP server on port 80
4. **Network Wait**: Waits up to 5 seconds for active network connection
5. **Status Display**: Prints application name and NNDK revision to console
6. **Main Loop**: Enters infinite loop with 1-second delays

### Web Content

The application serves a simple HTML page (`index.html`) that includes:
- NetBurner logo and branding
- Basic styling via CSS
- "Thank you for NetBurning!" message
- Link to NetBurner website

## Technical Details

### Dependencies

- **NBRTOS**: NetBurner Real-Time Operating System
- **Network Stack**: Built-in TCP/IP stack
- **HTTP Server**: Integrated web server functionality

### System Requirements

- NetBurner compatible hardware
- Network connectivity (Ethernet or WiFi depending on platform)
- NNDK (NetBurner Network Development Kit)

## Usage

1. **Compile**: Build the application using the NetBurner development environment
2. **Deploy**: Load the application onto your NetBurner device
3. **Connect**: Ensure the device is connected to your network
4. **Access**: Open a web browser and navigate to the device's IP address
5. **View**: The simple HTML page will be displayed

## Development Notes

### Production Considerations

- Remove `EnableSystemDiagnostics()` call for production deployments
- Customize the HTML content in `index.html` for your specific application
- Modify CSS styling in `style.css` to match your branding
- Add additional web pages and functionality as needed

### Customization

This example serves as a foundation for more complex web applications. You can:
- Add dynamic content generation
- Implement form handling
- Create RESTful APIs
- Add authentication and security features
- Integrate with sensors and other hardware

## Console Output

When running, the application displays:
```
Web Application: Simple HTML Example
NNDK Revision: [Version Information]
```

*/

